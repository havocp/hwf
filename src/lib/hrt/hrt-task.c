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
#include <hrt/hrt-watcher.h>
#include <hrt/hrt-marshalers.h>

#include <string.h>

typedef struct {
    char *name;
    GValue value;
} HrtTaskArg;

struct HrtTask {
    GObject      parent_instance;
    HrtTaskRunner *runner;
    volatile int watchers_count;
    GMutex *invoker_lock;
    HrtInvoker *invoker;
    gboolean completed;
    GSList *args;
    GValue result;
    GSList *completed_notifiees;
#ifndef G_DISABLE_CHECKS
    GThread *invoke_thread;
#endif
    /* Only present during invoke */
    HrtTaskThreadLocal *thread_local;
};

struct HrtTaskClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE(HrtTask, hrt_task, G_TYPE_OBJECT);

/* remember that adding props makes GObject a lot slower to
 * construct
 */
enum {
    PROP_0
};

enum  {
    LAST_SIGNAL
};

/* static guint signals[LAST_SIGNAL]; */

static void
hrt_task_get_property (GObject                *object,
                       guint                   prop_id,
                       GValue                 *value,
                       GParamSpec             *pspec)
{
    HrtTask *hrt_task;

    hrt_task = HRT_TASK(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
hrt_task_set_property (GObject                *object,
                       guint                   prop_id,
                       const GValue           *value,
                       GParamSpec             *pspec)
{
    HrtTask *hrt_task;

    hrt_task = HRT_TASK(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

void
_hrt_task_set_runner(HrtTask       *task,
                     HrtTaskRunner *runner)
{
    /* runner is not allowed to change.  task does not ref the runner
     * (runner refs task)
     */
    g_return_if_fail(task->runner == NULL ||
                     task->runner == runner);

    task->runner = runner;
}

HrtTaskRunner*
_hrt_task_get_runner(HrtTask *task)
{
    return task->runner;
}

HrtTask*
hrt_task_create_task(HrtTask *parent)
{
    HrtTask *task;

    task = g_object_new(HRT_TYPE_TASK, NULL);

    _hrt_task_set_runner(task, parent->runner);

    return task;
}

static HrtTaskArg*
hrt_task_arg_new(const char   *name,
                 const GValue *value)
{

    HrtTaskArg *arg;

    /* must use new0 to get 0-initialization for GValue */
    arg = g_slice_new0(HrtTaskArg);

    arg->name = g_strdup(name);
    g_value_init(&arg->value, G_VALUE_TYPE(value));
    g_value_copy(value, &arg->value);

    return arg;
}

static void
hrt_task_arg_free(HrtTaskArg *arg)
{
    g_free(arg->name);
    g_value_unset(&arg->value);

    g_slice_free(HrtTaskArg, arg);
}

/* args can only be added before there are any watchers so there's no
 * need for concurrency protection. Basically you can't be setting
 * args from the main thread when the task is already running. You
 * have to first set args, then start up the task. And after that
 * args are immutable.
 */
void
hrt_task_add_arg(HrtTask       *task,
                 const char    *name,
                 const GValue  *value)
{
    HrtTaskArg *arg;

    /* args have to be added before there are any watchers
     * on the task; otherwise we'd have a thread-safety headache.
     */
    g_return_if_fail(!_hrt_task_has_watchers(task));

#ifndef G_DISABLE_CHECKS
    {
        GSList *tmp;

        for (tmp = task->args;
             tmp != NULL;
             tmp = tmp->next) {
            arg = tmp->data;
            if (strcmp(arg->name, name) == 0) {
                g_critical("%s: Cannot set an already-added arg - args are immutable", G_STRFUNC);
                return;
            }
        }
    }
#endif

    arg = hrt_task_arg_new(name, value);
    task->args = g_slist_prepend(task->args, arg);
}

gboolean
hrt_task_get_arg(HrtTask       *task,
                 const char    *name,
                 GValue        *value,
                 GError       **error)
{
    HrtTaskArg *arg;
    GSList *tmp;

    for (tmp = task->args;
         tmp != NULL;
         tmp = tmp->next) {
        arg = tmp->data;
        if (strcmp(arg->name, name) == 0) {
            if (g_value_type_compatible(G_VALUE_TYPE(&arg->value),
                                        G_VALUE_TYPE(value))) {
                g_value_copy(&arg->value, value);
                return TRUE;
            } else {
                g_set_error(error, G_FILE_ERROR,
                            G_FILE_ERROR_FAILED,
                            "Requested arg '%s' expecting type '%s' but it has type '%s'",
                            name, G_VALUE_TYPE_NAME(value), G_VALUE_TYPE_NAME(&arg->value));
                return FALSE;
            }
        }
    }
    g_set_error(error, G_FILE_ERROR,
                G_FILE_ERROR_FAILED,
                "Task has no arg named '%s'",
                name);
    return FALSE;
}

void
hrt_task_get_args(HrtTask        *task,
                  char         ***names_p,
                  GValue        **values_p)
{
    int n_args;
    char **names;
    GValue *values;
    GSList *tmp;
    int i;

    n_args = g_slist_length(task->args);

    names = g_new0(char*, n_args + 1);
    names[n_args] = NULL;

    if (n_args > 0)
        values = g_new0(GValue, n_args);
    else
        values = NULL;

    for (i = 0, tmp = task->args; tmp != NULL; tmp = tmp->next, ++i) {
        HrtTaskArg *arg = tmp->data;
        names[i] = g_strdup(arg->name);
        g_value_init(&values[i], G_VALUE_TYPE(&arg->value));
        g_value_copy(&arg->value, &values[i]);
    }

    *names_p = names;
    *values_p = values;
}

/* The result can only be set once and is then immutable */
void
hrt_task_set_result(HrtTask       *task,
                    const GValue  *value)
{
#ifndef G_DISABLE_CHECKS
    if (G_VALUE_TYPE(&task->result) != 0) {
        g_warning("Cannot set task result twice");
        return;
    }
#endif
    g_value_init(&task->result, G_VALUE_TYPE(value));
    g_value_copy(value, &task->result);
}

gboolean
hrt_task_get_result(HrtTask       *task,
                    GValue        *value,
                    GError       **error)
{
    if (G_VALUE_TYPE(&task->result) == 0) {
        g_set_error(error, G_FILE_ERROR,
                    G_FILE_ERROR_FAILED,
                    "Task has no result set on it");
        return FALSE;
    } else if (g_value_type_compatible(G_VALUE_TYPE(&task->result),
                                       G_VALUE_TYPE(value))) {
        g_value_copy(&task->result, value);
        return TRUE;
    } else {
        g_set_error(error, G_FILE_ERROR,
                    G_FILE_ERROR_FAILED,
                    "Requested task result expecting type '%s' but it has type '%s'",
                    G_VALUE_TYPE_NAME(value), G_VALUE_TYPE_NAME(&task->result));
        return FALSE;
    }
}

void*
hrt_task_get_thread_local(HrtTask        *task,
                          void           *key)
{
    if (task->thread_local == NULL) {
        g_warning("Can only use thread local during a task invoke");
        return NULL;
    }

    return _hrt_task_thread_local_get(task->thread_local, key);
}

void
hrt_task_set_thread_local(HrtTask        *task,
                          void           *key,
                          void           *value,
                          GDestroyNotify  dnotify)
{
    if (task->thread_local == NULL) {
        g_warning("Can only use thread local during a task invoke");
        return;
    }

    _hrt_task_thread_local_set(task->thread_local,
                               key, value, dnotify);
}

void
hrt_task_block_completion(HrtTask *task)
{
    /* Task won't complete while it has watchers. We don't use watcher
     * count for anything other than checking if we can complete, so
     * just create a fake watcher count to stop completion.
     */
    _hrt_task_watchers_inc(task);
}

void
hrt_task_unblock_completion(HrtTask *task)
{
    /* don't call _hrt_task_watchers_dec() because it asserts that
     * we're in the task thread. (actual watchers can't be removed
     * from the main thread.)
     */
    g_atomic_int_add(&task->watchers_count, -1);

    /* if that was our last watcher we now have to nominate
     * ourselves to be completed in main thread.
     */
    if (!_hrt_task_has_watchers(task)) {
        _hrt_task_runner_queue_completed_task(task->runner, task);
    }
}

HrtWatcher*
hrt_task_add_immediate(HrtTask              *task,
                       HrtWatcherCallback    callback,
                       void                 *data,
                       GDestroyNotify        dnotify)
{
    return _hrt_task_runner_add_immediate(task->runner,
                                          task,
                                          callback,
                                          data,
                                          dnotify);
}

HrtWatcher*
hrt_task_add_idle(HrtTask              *task,
                  HrtWatcherCallback    callback,
                  void                 *data,
                  GDestroyNotify        dnotify)
{
    return _hrt_task_runner_add_idle(task->runner,
                                     task,
                                     callback,
                                     data,
                                     dnotify);
}

HrtWatcher*
hrt_task_add_io(HrtTask              *task,
                int                   fd,
                HrtWatcherFlags       io_flags,
                HrtWatcherCallback    callback,
                void                 *data,
                GDestroyNotify        dnotify)
{
    return _hrt_task_runner_add_io(task->runner,
                                   task,
                                   fd,
                                   io_flags,
                                   callback,
                                   data,
                                   dnotify);
}

HrtWatcher*
hrt_task_add_subtask(HrtTask              *task,
                     HrtTask              *wait_for_completed,
                     HrtWatcherCallback    callback,
                     void                 *data,
                     GDestroyNotify        dnotify)
{
    return _hrt_task_runner_add_subtask(task->runner,
                                        task,
                                        wait_for_completed,
                                        callback,
                                        data,
                                        dnotify);
}

gboolean
hrt_task_check_in_task_thread(HrtTask *task)
{
    return g_thread_self() == task->invoke_thread;
}

gboolean
hrt_task_check_in_task_thread_or_completed(HrtTask *task)
{
    return _hrt_task_is_completed(task) ||
        hrt_task_check_in_task_thread(task);
}

static void
hrt_task_dispose(GObject *object)
{
    HrtTask *hrt_task;
    GSList *tmp;

    hrt_task = HRT_TASK(object);

    for (tmp = hrt_task->args; tmp != NULL; tmp = tmp->next) {
        HrtTaskArg *arg = tmp->data;
        hrt_task_arg_free(arg);
    }
    g_slist_free(hrt_task->args);

    if (G_VALUE_TYPE(&hrt_task->result) != 0) {
        g_value_unset(&hrt_task->result);
    }

    G_OBJECT_CLASS(hrt_task_parent_class)->dispose(object);
}

static void
hrt_task_finalize(GObject *object)
{
    HrtTask *hrt_task;

    hrt_task = HRT_TASK(object);

    g_assert(hrt_task->invoker == NULL);
    g_assert(hrt_task->completed_notifiees == NULL);

    g_mutex_free(hrt_task->invoker_lock);

    G_OBJECT_CLASS(hrt_task_parent_class)->finalize(object);
}

void
_hrt_task_lock_invoker(HrtTask *task)
{
    g_mutex_lock(task->invoker_lock);
}

void
_hrt_task_unlock_invoker(HrtTask *task)
{
    g_mutex_unlock(task->invoker_lock);
}

HrtInvoker*
_hrt_task_get_invoker(HrtTask *task)
{
    return task->invoker;
}

void
_hrt_task_set_invoker(HrtTask       *task,
                      HrtInvoker    *invoker)
{
    task->invoker = invoker;
}

void
_hrt_task_enter_invoke(HrtTask            *task,
                       HrtTaskThreadLocal *thread_local)
{
#ifndef G_DISABLE_CHECKS
    task->invoke_thread = g_thread_self();
#endif
    task->thread_local = thread_local;
}

void
_hrt_task_leave_invoke(HrtTask *task)
{
#ifndef G_DISABLE_CHECKS
    task->invoke_thread = NULL;
#endif
    task->thread_local = NULL;
}

void
_hrt_task_watchers_inc(HrtTask *task)
{
    g_assert(!_hrt_task_is_completed(task));

    g_atomic_int_inc(&task->watchers_count);
}

void
_hrt_task_watchers_dec(HrtTask *task)
{
    /* Watchers should be removed from a special "remove watcher" that
     * gets run in the task thread.
     */
    HRT_ASSERT_IN_TASK_THREAD(task);

    g_atomic_int_add(&task->watchers_count, -1);
}

gboolean
_hrt_task_has_watchers(HrtTask *task)
{
    return g_atomic_int_get(&task->watchers_count) > 0;
}

/* abuse the invoker lock. no reason we should need to add/remove
 * completed notifies while holding the invoker lock.
 */
#define LOCK_COMPLETED_NOTIFIEES(task)          \
    g_mutex_lock((task)->invoker_lock)
#define UNLOCK_COMPLETED_NOTIFIEES(task)        \
    g_mutex_unlock((task)->invoker_lock)

/* RUN IN MAIN THREAD */
void
_hrt_task_mark_completed(HrtTask *task)
{
    g_assert(g_atomic_int_get(&task->watchers_count) == 0);
    g_assert(task->invoker == NULL);

    if (!task->completed) {
        task->completed = TRUE;

        LOCK_COMPLETED_NOTIFIEES(task);
        while (task->completed_notifiees != NULL) {
            HrtWatcher *notifiee = task->completed_notifiees->data;

            task->completed_notifiees =
                g_slist_remove(task->completed_notifiees,
                               task->completed_notifiees->data);

            UNLOCK_COMPLETED_NOTIFIEES(task);
            _hrt_watcher_subtask_notify(notifiee);
            LOCK_COMPLETED_NOTIFIEES(task);
        }
        UNLOCK_COMPLETED_NOTIFIEES(task);
    }
}

gboolean
_hrt_task_is_completed(HrtTask *task)
{
    return task->completed;
}

/* RUN FROM ANY THREAD */
void
_hrt_task_add_completed_notify(HrtTask    *task,
                               HrtWatcher *subtask_watcher)
{
    LOCK_COMPLETED_NOTIFIEES(task);
    /* subtask_watcher will have a pointer to task and
     * remove itself on finalize, so this is a weak ref
     */
    task->completed_notifiees =
        g_slist_prepend(task->completed_notifiees,
                        subtask_watcher);
    UNLOCK_COMPLETED_NOTIFIEES(task);
}

/* RUN FROM ANY THREAD */
void
_hrt_task_remove_completed_notify(HrtTask    *task,
                                  HrtWatcher *subtask_watcher)
{
    LOCK_COMPLETED_NOTIFIEES(task);
    task->completed_notifiees =
        g_slist_remove(task->completed_notifiees,
                       subtask_watcher);
    UNLOCK_COMPLETED_NOTIFIEES(task);
}

static void
hrt_task_init(HrtTask *hrt_task)
{
    hrt_task->invoker_lock = g_mutex_new();
}

static void
hrt_task_class_init(HrtTaskClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = hrt_task_get_property;
    object_class->set_property = hrt_task_set_property;

    object_class->dispose = hrt_task_dispose;
    object_class->finalize = hrt_task_finalize;
}
