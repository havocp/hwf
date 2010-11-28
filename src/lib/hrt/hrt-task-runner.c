/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2010 Havoc Pennington
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <config.h>

#include <hrt/hrt-task-private.h>

#include <hrt/hrt-log.h>
#include <hrt/hrt-event-loop.h>
#include <hrt/hrt-thread-pool.h>
#include <hrt/hrt-watcher.h>
#include <hrt/hrt-builtins.h>
#include <hrt/hrt-marshalers.h>

struct HrtTaskRunner {
    GObject      parent_instance;

    /* Context of the "main" thread (the one
     * the runner was created in)
     */
    GMainContext *runner_context;

    /* A thread dedicated to blocking in the main loop and then
     * dumping any resulting events into invoke threads.
     * The main loop in the event thread may not be a glib
     * main loop.
     */
    GThread *event_thread;

    /* The event loop run in the event thread. */
    HrtEventLoop *event_loop;

    /* Thread pool used to invoke handlers for main
     * loop events, carefully invoking only one handler
     * at a time for each HrtTask.
     */
    HrtThreadPool *invoke_threads;

    /* We complete tasks in the runner_context (main thread) by adding
     * them to this queue and draining the queue in an idle in the
     * runner context. The queue has two advantages over just adding
     * an idle for each task to complete: first, we avoid taking the
     * runner_context lock all the time. Second, we avoid the overhead
     * of GSource and especially the O(n)
     * g_source_attach(). https://bugzilla.gnome.org/show_bug.cgi?id=619329
     *
     * We use a tasks-completed signal on HrtTaskRunner that handles
     * multiple tasks, rather than a completed signal on each task,
     * because it's more efficient. The signals on individual HrtTask
     * were definitely showing up in profiles when we tried that.
     */
    GQueue  completed_tasks;
    guint   completed_tasks_idle_id;
    GMutex *completed_tasks_lock;

    /* in the idle, we copy completed_tasks to here,
     * and then the main thread pulls from here.
     */
    GQueue  unlocked_completed_tasks;
};

struct HrtTaskRunnerClass {
    GObjectClass parent_class;
};


G_DEFINE_TYPE(HrtTaskRunner, hrt_task_runner, G_TYPE_OBJECT);

enum {
    PROP_0,
    PROP_EVENT_LOOP_TYPE
};

enum  {
    TASKS_COMPLETED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void
hrt_task_runner_get_property (GObject                *object,
                              guint                   prop_id,
                              GValue                 *value,
                              GParamSpec             *pspec)
{
    HrtTaskRunner *runner;

    runner = HRT_TASK_RUNNER(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_task_runner_set_property (GObject                *object,
                              guint                   prop_id,
                              const GValue           *value,
                              GParamSpec             *pspec)
{
    HrtTaskRunner *runner;

    runner = HRT_TASK_RUNNER(object);

    switch (prop_id) {
    case PROP_EVENT_LOOP_TYPE: {
        HrtEventLoopType loop_type;

        g_return_if_fail(runner->event_loop == NULL);

        loop_type = g_value_get_enum(value);
        runner->event_loop = _hrt_event_loop_new(loop_type);
    }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_task_runner_dispose(GObject *object)
{
    HrtTaskRunner *runner;

    runner = HRT_TASK_RUNNER(object);

    if (runner->event_loop) {
        HrtEventLoop *loop = runner->event_loop;

        runner->event_loop = NULL;

        /* loop should already be running so this returns immediately,
         * or if things are broken it hangs here which should give a
         * clear backtrace showing the problem.
         */
        _hrt_event_loop_wait_running(loop, TRUE);

        _hrt_event_loop_quit(loop);

        /* this isn't really needed, we could just join the thread,
         * but this gives a clearer backtrace if things go wrong and
         * we don't quit.
         */
        _hrt_event_loop_wait_running(loop, FALSE);

        g_thread_join(runner->event_thread);
        runner->event_thread = NULL;

        g_object_unref(loop);
    }

    if (runner->invoke_threads) {
        hrt_thread_pool_shutdown(runner->invoke_threads);
        g_object_unref(runner->invoke_threads);
        runner->invoke_threads = NULL;
    }

    /* the definition of dispose would usually involve freeing
     * everything in completed_tasks but we wait for the idle
     * to do that to be sure it's in the right thread. In the
     * meantime there should be a ref held by the idle preventing
     * finalization.
     */

    G_OBJECT_CLASS(hrt_task_runner_parent_class)->dispose(object);
}

static void
hrt_task_runner_finalize(GObject *object)
{
    HrtTaskRunner *runner;

    runner = HRT_TASK_RUNNER(object);

    /* the completed tasks idle has to complete before we can
     * finalize.
     */
    g_assert(g_queue_get_length(&runner->completed_tasks) == 0);
    g_assert(runner->completed_tasks_idle_id == 0);
    g_queue_clear(&runner->completed_tasks);
    g_mutex_free(runner->completed_tasks_lock);
    /* the idle should have cleared this too. */
    g_assert(g_queue_get_length(&runner->unlocked_completed_tasks) == 0);
    g_queue_clear(&runner->unlocked_completed_tasks);

    G_OBJECT_CLASS(hrt_task_runner_parent_class)->finalize(object);
}


/* Watcher/Invoker/etc. What Is Going On
 *
 * - we need each task to be run from only one thread at a time,
 *   across all that task's watchers, so that task code
 *   doesn't have to deal with multiple threads
 * - when we async handle an event, we don't want the event
 *   to fire again and pile up. In particular, we don't want
 *   an IO event to be triggered as ready for reading when
 *   we just haven't gotten around to read() yet.
 * - the way it works is:
 *
 *  - when a watcher fires, ensure an queue exists for its events.
 *    this is the "invoker"
 *  - block the watcher
 *  - add event to invoker queue
 *  - if the task is not in the invoke pool, add it
 *  - inside invoke thread, task will pop event from invoker queue
 *  - invoke event
 *  - unblock the watcher
 *    (optimization: special-case IO watcher by polling in-place)
 *    (at this point watcher can be re-invoked and re-queued at any time)
 *  - when queue is drained, remove ourselves from invoke pool
 *
 * In libev, it looks like if we block/unblock an IO watcher before
 * the next iteration, we will never actually mess with the epoll.
 * This optimization could be win.
 */


struct HrtInvoker {
    volatile int refcount;

    HrtTask *task;

    GMutex *pending_watchers_lock;
    GQueue pending_watchers;

};

static HrtInvoker*
hrt_invoker_new(HrtTask    *task,
                HrtWatcher *first_watcher)
{
    HrtInvoker *invoker;

    invoker = g_slice_new(HrtInvoker);

    invoker->refcount = 1;
    invoker->task = task;
    g_object_ref(task);

    invoker->pending_watchers_lock = g_mutex_new();
    g_queue_init(&invoker->pending_watchers);

    /* passing in first_watcher lets us avoid locking the queue for
     * just one watcher which is probably the most common case.
     * Using GStaticMutex we could even potentially
     * avoid creating a mutex, but for now skipping that.
     */
    _hrt_watcher_ref(first_watcher);
    g_queue_push_tail(&invoker->pending_watchers, first_watcher);

    return invoker;
}

#if 0
/* unused */
static void
hrt_invoker_ref(HrtInvoker *invoker)
{
    g_atomic_int_inc(&invoker->refcount);
}
#endif

static void
hrt_invoker_unref(HrtInvoker *invoker)
{
    if (g_atomic_int_dec_and_test(&invoker->refcount)) {
        g_object_unref(invoker->task);

        g_mutex_free(invoker->pending_watchers_lock);

        g_assert(g_queue_get_length(&invoker->pending_watchers) == 0);

        g_slice_free(HrtInvoker, invoker);
    }
}


static void
hrt_invoker_queue_watcher(HrtInvoker *invoker,
                          HrtWatcher *watcher)
{
    _hrt_watcher_ref(watcher);

    g_mutex_lock(invoker->pending_watchers_lock);
    g_queue_push_tail(&invoker->pending_watchers, watcher);
    g_mutex_unlock(invoker->pending_watchers_lock);
}

/* returns a ref if not NULL*/
static HrtWatcher*
hrt_invoker_pop_watcher(HrtInvoker *invoker)
{
    HrtWatcher *watcher;

    g_mutex_lock(invoker->pending_watchers_lock);
    watcher = g_queue_pop_head(&invoker->pending_watchers);
    g_mutex_unlock(invoker->pending_watchers_lock);

    return watcher;
}

static gboolean
hrt_invoker_has_watchers(HrtInvoker *invoker)
{
    gboolean has_watchers;

    g_mutex_lock(invoker->pending_watchers_lock);
    has_watchers = g_queue_get_length(&invoker->pending_watchers) > 0;
    g_mutex_unlock(invoker->pending_watchers_lock);

    return has_watchers;
}

/* IN EVENT OR INVOKE THREADS */
void
_hrt_task_runner_watcher_pending(HrtTaskRunner      *runner,
                                 HrtWatcher         *watcher)
{
    HrtInvoker *invoker;
    gboolean invoker_created;

    _hrt_task_lock_invoker(watcher->task);
    invoker = _hrt_task_get_invoker(watcher->task);
    if (invoker == NULL) {
        invoker = hrt_invoker_new(watcher->task, watcher);
        _hrt_task_set_invoker(watcher->task, invoker);
        invoker_created = TRUE;
    } else {
        hrt_invoker_queue_watcher(invoker, watcher);
        invoker_created = FALSE;
    }

    if (invoker_created) {
        hrt_thread_pool_push(runner->invoke_threads,
                             invoker);
    }

    /* If we didn't create invoker, we rely on it still being
     * running in the thread pool.
     */
    _hrt_task_unlock_invoker(watcher->task);
}

HrtEventLoop*
_hrt_task_runner_get_event_loop(HrtTaskRunner *runner)
{
    return runner->event_loop;
}

/* Immediately queue the callback for the invoke thread,
 * i.e. does not wait until the next main loop iteration.
 * dnotify will also run in the invoke thread.
 */
HrtWatcher*
_hrt_task_runner_add_immediate(HrtTaskRunner      *runner,
                               HrtTask            *task,
                               HrtWatcherCallback  callback,
                               void               *data,
                               GDestroyNotify      dnotify)
{
    HrtWatcher *watcher;

    g_return_val_if_fail(_hrt_task_get_runner(task) == runner, NULL);

    watcher =
        _hrt_watcher_new_immediate(task, callback, data, dnotify);

    /* start() on immediate watcher just queues for immediate
     * invoke.
     */
    _hrt_watcher_start(watcher);

    return watcher;

}

/* dnotify is called from an invoke thread.
 * func is called from an invoke thread too.
 */
HrtWatcher*
_hrt_task_runner_add_idle(HrtTaskRunner     *runner,
                          HrtTask           *task,
                          HrtWatcherCallback func,
                          void              *data,
                          GDestroyNotify     dnotify)
{
    HrtWatcher *watcher;

    g_return_val_if_fail(_hrt_task_get_runner(task) == runner, NULL);

    watcher =
        _hrt_event_loop_create_idle(runner->event_loop,
                                    task, func, data, dnotify);

    /* the watcher can already be invoked, or removed, in another
     * thread as soon as we call this.
     */
    _hrt_watcher_start(watcher);

    return watcher;
}

HrtWatcher*
_hrt_task_runner_add_io(HrtTaskRunner      *runner,
                        HrtTask            *task,
                        int                 fd,
                        HrtWatcherFlags     io_flags,
                        HrtWatcherCallback  func,
                        void               *data,
                        GDestroyNotify      dnotify)
{
    HrtWatcher *watcher;

    g_return_val_if_fail(_hrt_task_get_runner(task) == runner, NULL);

    watcher =
        _hrt_event_loop_create_io(runner->event_loop,
                                  task, fd, io_flags,
                                  func, data, dnotify);

    /* the watcher can already be invoked, or removed, in another
     * thread as soon as we call this.
     */
    _hrt_watcher_start(watcher);

    return watcher;
}

HrtWatcher*
_hrt_task_runner_add_subtask(HrtTaskRunner      *runner,
                             HrtTask            *task,
                             HrtTask            *wait_for_completed,
                             HrtWatcherCallback  callback,
                             void               *data,
                             GDestroyNotify      dnotify)
{
    HrtWatcher *watcher;

    g_return_val_if_fail(_hrt_task_get_runner(task) == runner, NULL);

    /* catch trivial cycle. just hope people won't shoot themselves in
     * foot with a complex cycle.
     */
    g_return_val_if_fail(task != wait_for_completed, NULL);

    watcher =
        _hrt_watcher_new_subtask(task, wait_for_completed,
                                 callback, data, dnotify);

    _hrt_watcher_start(watcher);

    return watcher;
}

/* Creates a new task, owned by the caller, associated with
 * the task runner.
 */
HrtTask*
hrt_task_runner_create_task(HrtTaskRunner *runner)
{
    HrtTask *task;

    task = g_object_new(HRT_TYPE_TASK, NULL);

    _hrt_task_set_runner(task, runner);

    return task;
}

/* If invoker was non-NULL, we know another completion will be queued
 * on the task, because we always queue a completion when we unset an
 * invoker and watchers have dropped to 0.  If it's NULL, _and_
 * watchers are zero, we know another completion will not be queued
 * because there won't be watcher callbacks from invoke thread to
 * queue them, and we're in the main thread and we won't queue them.
 */
static gboolean
invoker_was_null(HrtTask *task)
{
    gboolean is_null;

    _hrt_task_lock_invoker(task);
    is_null = _hrt_task_get_invoker(task) == NULL;
    _hrt_task_unlock_invoker(task);

    return is_null;
}

/* RUN IN MAIN THREAD */
/* Note: this returns ownership of the task. */
HrtTask*
hrt_task_runner_pop_completed(HrtTaskRunner *runner)
{
    HrtTask *task;

    /* The tricky case here is if you create a task, add several
     * watchers, then enter main loop.  We could have one watcher run,
     * then we queue this completion idle, then add two more watchers.
     * In that case we don't want to emit completed.  We would no-op
     * here, and when the watcher count goes back to zero, the task
     * will be re-queued for completion.  Some inefficiency in
     * potentially queuing the task for completion more than once, but
     * no big deal hopefully.
     *
     * The basic effect here is that a task is only completed when it
     * has zero watchers, and STILL has zero watchers when the main
     * thread mainloop runs.
     *
     * It's also possible that we add watcher, run it, queue complete,
     * add watcher, run it, queue complete. Then we have the same task
     * in the queue twice. In that case, we rely on skipping any
     * tasks that are already marked completed.
     */

    while ((task = g_queue_pop_head(&runner->unlocked_completed_tasks)) != NULL) {
        /* We skip the task if watchers were added (so it's no longer
         * complete) or if it was already completed (so we don't want
         * to return it again). Also we skip it if invoker is non-NULL
         * which means watcher count may be 0 but invoker hasn't been
         * unset; in this case we know the invoke thread will queue
         * for completion again on unset, so we can skip this node.
         *
         * Remember watchers can be added from an invoke thread or the
         * main thread, and completion can only happen in main thread.
         * Since we're in the main thread now, watchers could
         * concurrently be added in the invoke thread. However, invoke
         * threads only run code and add watchers _from inside
         * watchers_, so if watchers are zero, we know they could only
         * become nonzero due to the main thread, which we're in.
         * Bottom line if watchers are 0 then they won't become
         * nonzero here.
         */

        if (!_hrt_task_is_completed(task) &&
            !_hrt_task_has_watchers(task) &&
            invoker_was_null(task)) {
            _hrt_task_mark_completed(task);
            /* return our ref to the task */
            return task;
        }

        /* drop queue's ref */
        g_object_unref(task);
    }

    return NULL;
}

static gboolean
complete_tasks_in_runner_thread(void *data)
{
    HrtTaskRunner *runner = HRT_TASK_RUNNER(data);
    HrtTask *task;

    g_mutex_lock(runner->completed_tasks_lock);

    /* unlocked_completed_tasks must be empty because we only run one
     * of these idles at a time, and at the end of the idle we
     * always empty unlocked_completed_tasks if the app did not.
     */
    g_assert(g_queue_get_length(&runner->unlocked_completed_tasks) == 0);

    /* just copy entire GQueue by value */
    runner->unlocked_completed_tasks = runner->completed_tasks;
    g_queue_init(&runner->completed_tasks);

    runner->completed_tasks_idle_id = 0;

    g_mutex_unlock(runner->completed_tasks_lock);

    /* During this emission, unlocked_completed_tasks MUST be drained or
     * else we'll just drop its contents on the floor.
     */
    g_signal_emit(G_OBJECT(runner), signals[TASKS_COMPLETED], 0);

    /* drop any tasks that weren't taken by the app on the floor */
    while ((task = hrt_task_runner_pop_completed(runner)) != NULL) {
        g_object_unref(task);
    }

    return FALSE;
}

void
_hrt_task_runner_queue_completed_task(HrtTaskRunner *runner,
                                      HrtTask       *task)
{
    /* We want to emit completed signal in the main (runner)
     * thread. The completion idle does nothing if a watcher is added
     * before the completion idle runs.
     */
    g_assert(!_hrt_task_is_completed(task));

    g_mutex_lock(runner->completed_tasks_lock);

    if (runner->completed_tasks_idle_id == 0) {
        GSource *source;
        source = g_idle_source_new();
        g_object_ref(runner);
        g_source_set_callback(source, complete_tasks_in_runner_thread,
                              runner, (GDestroyNotify) g_object_unref);
        g_source_attach(source, runner->runner_context);
        runner->completed_tasks_idle_id = g_source_get_id(source);
        g_source_unref(source);
    }

    g_object_ref(task);
    g_queue_push_tail(&runner->completed_tasks, task);

    g_mutex_unlock(runner->completed_tasks_lock);
}


static void*
invoke_pool_thread_data_new(void *vfunc_data)
{
    return _hrt_task_thread_local_new();
}

static void
invoke_pool_handle_item (void *thread_data,
                         void *pushed_item,
                         void *pool_data)
{
    HrtTaskThreadLocal *thread_local = thread_data;
    HrtInvoker *invoker = pushed_item;
    HrtTaskRunner *runner = HRT_TASK_RUNNER(pool_data);
    HrtWatcher *watcher;
    HrtTask *task;

    task = invoker->task;

    g_object_ref(task);

 redrain_watchers:
    g_assert(!_hrt_task_is_completed(task));

    while ((watcher = hrt_invoker_pop_watcher(invoker)) != NULL) {
        HrtWatcherCallback func;
        void *watcher_data;
        gboolean restart;

        g_assert(!_hrt_task_is_completed(task));

        /* Each event either fired, or the watcher was removed.
         * Removal can happen during the firing, too. We don't
         * want to run events on removed watchers since people
         * won't expect to remove a timeout then get a callback
         * from it... which can happen due to the queue.
         */
        if (g_atomic_int_get(&watcher->removed) > 0) {
            continue;
        }

        func = watcher->func;
        watcher_data = watcher->data;

        _hrt_task_enter_invoke(task, thread_local);
        restart = (* func) (task,
                            watcher->flags,
                            watcher_data);
        _hrt_task_leave_invoke(task);

        watcher->flags = HRT_WATCHER_FLAG_NONE;

        if (!restart &&
            g_atomic_int_get(&watcher->removed) == 0) {
            /* rather than manually removing, callback can
             * return FALSE to remove
             */
            hrt_watcher_remove(watcher);
        }

        if (restart &&
            g_atomic_int_get(&watcher->removed) == 0) {
            /* unblock the watcher if it wasn't removed during
             * the callback. There should be no way that it
             * gets removed from the main or event threads.
             * If other threads could remove then we have a
             * race here between testing "removed" and adding
             * back to the main loop.
             */
            _hrt_watcher_start(watcher);
        }

        _hrt_watcher_unref(watcher);
    }

    g_assert(!_hrt_task_is_completed(task));

    _hrt_task_lock_invoker(task);

    /* The idea here is that while the invoker is still
     * available, we guarantee we'll process any watcher
     * events added to it.
     */
    _hrt_task_set_invoker(task, NULL);

    if (hrt_invoker_has_watchers(invoker)) {
        /* put invoker back and handle the watchers that were added... */
        _hrt_task_set_invoker(task, invoker);
        _hrt_task_unlock_invoker(task);
        goto redrain_watchers;
    }

    g_assert(!_hrt_task_is_completed(task));

    /* invoker is now cleared off the task and has no events. We can
     * queue completion.
     */

    /* We are worried about the following:
     *
     *  - invoke thread locks, sets invoker=NULL, and unlocks
     *  - invoke thread queues for completion
     *  - main thread adds a new watcher
     *  - event thread sets invoker to handle watcher
     *  - invoke thread runs the watcher and decrements to 0 watchers
     *  - main thread pops completed task and marks completed
     *  - invoke thread locks, sets invoker=NULL, and unlocks
     *  - invoke thread queues for completion
     *  - main thread pops completed task and marks completed AGAIN
     *    which is an error
     *
     * To avoid this, we queue completion with invoker lock held, and
     * when popping completion we require that invoker is NULL
     * (checking with the lock held). If invoker is not NULL the main
     * thread knows we'll queue the completion again so it skips
     * completing.  If it is NULL, the main thread knows no more
     * completions can be queued from the invoke thread, because
     * there's no invoker and no watchers to queue from, and it knows
     * it won't queue a completion itself. So it can complete exactly
     * once.
     */
    if (!_hrt_task_has_watchers(task)) {
        /* task is completed when it has had a watcher once, and now
         * has none, and main thread has entered the main loop.
         * Completion is only done in the main thread.  Main thread
         * can add watchers again once we're already queued for
         * completion, in that case the main thread won't remove this
         * task in hrt_task_runner_pop_completed() and the task will
         * be resurrected.
         */
        _hrt_task_runner_queue_completed_task(runner, task);
    }

    _hrt_task_unlock_invoker(task);

    hrt_invoker_unref(invoker);

    g_object_unref(task);
}

static void
invoke_pool_thread_data_free(void *thread_data,
                             void *vfunc_data)
{
    _hrt_task_thread_local_free(thread_data);
}

static const HrtThreadPoolVTable invoke_pool_vtable = {
    invoke_pool_thread_data_new,
    invoke_pool_handle_item,
    invoke_pool_thread_data_free
};

static void*
task_runner_event_thread(void *data)
{
    HrtTaskRunner *runner = HRT_TASK_RUNNER(data);

    _hrt_event_loop_run(runner->event_loop);

    return NULL;
}

static void
hrt_task_runner_init(HrtTaskRunner *runner)
{
    /* Most stuff is in the constructor so the event loop type
     * will have been set.
     */

    g_queue_init(&runner->completed_tasks);
    g_queue_init(&runner->unlocked_completed_tasks);
    runner->completed_tasks_lock = g_mutex_new();
}

static GObject*
hrt_task_runner_constructor(GType                  type,
                            guint                  n_construct_properties,
                            GObjectConstructParam *construct_params)
{
    GObject *object;
    HrtTaskRunner *runner;
    GError *error;

    object = G_OBJECT_CLASS(hrt_task_runner_parent_class)->constructor(type,
                                                                       n_construct_properties,
                                                                       construct_params);

    runner = HRT_TASK_RUNNER(object);

    g_assert(runner->event_loop != NULL);

    /* note that runner_context is NULL if it's the
     * global default context.
     */
    runner->runner_context =
        g_main_context_get_thread_default();

    error = NULL;
    runner->invoke_threads =
        hrt_thread_pool_new(&invoke_pool_vtable,
                            runner,
                            NULL);

    error = NULL;
    runner->event_thread =
        g_thread_create(task_runner_event_thread, runner,
                        TRUE, &error);
    if (error != NULL) {
        g_error("create thread: %s", error->message);
    }

    /* wait for main loop to be running in event thread. This avoids
     * races, such as whether it's OK to quit the main loop in
     * dispose().
     */
    _hrt_event_loop_wait_running(runner->event_loop, TRUE);

    return object;
}

static void
hrt_task_runner_class_init(HrtTaskRunnerClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->constructor = hrt_task_runner_constructor;
    object_class->get_property = hrt_task_runner_get_property;
    object_class->set_property = hrt_task_runner_set_property;

    object_class->dispose = hrt_task_runner_dispose;
    object_class->finalize = hrt_task_runner_finalize;

    g_object_class_install_property(object_class,
                                    PROP_EVENT_LOOP_TYPE,
                                    g_param_spec_enum("event-loop-type",
                                                      "Event loop implementation",
                                                      "The event thread has its own loop, with this implementation",
                                                      HRT_TYPE_EVENT_LOOP_TYPE,
                                                      HRT_EVENT_LOOP_GLIB,
                                                      G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY));

    signals[TASKS_COMPLETED] =
        g_signal_new("tasks-completed",
                     G_OBJECT_CLASS_TYPE(klass),
                     G_SIGNAL_RUN_LAST,
                     0,
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}
