///////////////////////////////////////////////////////////////////////////////
//  COPYRIGHT (c) 2009 Schweitzer Engineering Laboratories, Pullman, WA
///////////////////////////////////////////////////////////////////////////////
//  Permission is hereby granted, free of charge, to any person
//  obtaining a copy of this software and associated documentation
//  files (the "Software"), to deal in the Software without
//  restriction, including without limitation the rights to use,
//  copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following
//  conditions:
//
//  The above copyright notice and this permission notice shall be
//  included in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
//  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
//  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
//  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
//  OTHER DEALINGS IN THE SOFTWARE.
///////////////////////////////////////////////////////////////////////////////
///  @file main.cpp
///
///  @brief
///  Drives the selcov program. It will find all gcno and gcda files in the
///  project directory, pair the correct files together, and output a text
///  coverage and dump file for each source file.
///
///  Requirements Specification:
///     < http://selcov.eecs.wsu.edu/wiki/index.php/SRS >
///
///  Design Description: < Null >
///
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES

#include <fstream>
#include <cstdlib>

// PROJECT INCLUDES

#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>
#include <boost/token_functions.hpp>

// LOCAL INCLUDES

#include "command.h"
#include "config.h"
//#include "Utility_Manager.h"

// FORWARD REFERENCES

namespace PO = boost::program_options;

using boost::char_separator;
using boost::tokenizer;

using std::vector;
using std::string;
using std::cout;
using std::endl;
using std::flush;
using std::ifstream;
using std::cerr;
using std::stringstream;
using std::exception;

// GLOBAL VARIABLES

static const string Help = "help";
static const string Help_config = "help-config";
static const string Version = "version";

// FUNCTION PROTOTYPES

int boost_cmd_handler( int ac, char* av[] );
void print_help();
void cmd_help();
void config_help();
void cmd_config_help();

int boost_cmd_handler(int ac, char* av[])
{
    
    Config & config = Config::get_instance();
    
    try {
        PO::options_description cmd_options("Command Line Options");
        cmd_options.add_options()
        ((Help + ",h").c_str(), "produce general help message.\n")
        (Help_config.c_str(), "produce help for options.\n")
        ((config.get_flag_name(Config::Working_directory) + ",c").c_str(), PO::value<string>(), 
             "specify the directory where Trucov will run.\n")
        ((config.get_flag_name(Config::Output) + ",o").c_str(), PO::value<string>(), 
            "specify the output directory where trucov should use.\n")
        (Config::Selection.c_str(), PO::value< vector<string> >(), 
            "specify the function(s) in all source files.\n")
        ((Version + ",v").c_str(), 
            "prints out the working version number of Trucov.\n")
        ((config.get_flag_name(Config::Debug) + ",d").c_str(), 
            "printout debug and testing information.\n")
        (config.get_flag_name(Config::Secret_gui).c_str(), "advanced use only.\n")
        (config.get_flag_name(Config::Brief).c_str(), 
            "only prints function summaries in coverage reports.\n")
        (config.get_flag_name(Config::Only_missing).c_str(), 
            "only outputs coverage information on functions with less than 100% coverage.\n")
        (config.get_flag_name(Config::Show_fake).c_str(), 
            "outputs coverage information on fake arcs and fake blocks.\n")
        (config.get_flag_name(Config::Hide_fake).c_str(), 
            "hides coverage information on fake arcs and fake blocks.\n")
        (config.get_flag_name(Config::Mangle).c_str(), 
            "outputs the mangled names of functions.\n")
        (config.get_flag_name(Config::Demangle).c_str(), 
            "outputs the demangled names of functions.\n")
        (config.get_flag_name(Config::Partial_match).c_str(), 
            "specify selection will be matched partially.\n")
        (config.get_flag_name(Config::Full_match).c_str(), 
            "specify selection will be matched fully.\n")
        (config.get_flag_name(Config::Signature_match).c_str(), 
            "specifies selection for functions shall be matched using the full function signature.\n")
        (config.get_flag_name(Config::Config_file).c_str(), po::value<string>(), 
            "specify the config file.\n")
        ;

        PO::options_description config_options("Config File Options");
        config_options.add_options()
        ("default-command", PO::value<string>(), 
            "the command used when no command is specified.\n")
        ("default-selection", PO::value< vector<string> >(), 
            "the selection used when no selection is specified.\n")
        ;

        PO::options_description cmd_config("");
        cmd_config.add_options()
       ((Config::Build_directory + ",b").c_str(), PO::value< vector<string> >(),
            "specify the root directorie(s) of the GCNO and GCDA files.\n")
       ((Config::Source_directory + ",s").c_str(), PO::value< vector<string> >(),
            "specify the directorie(s) of the source files.\n")
       ((config.get_flag_name(Config::Cache_file) + ",f").c_str(), PO::value<string>(), 
            "specify the cache file to be used.\n")
        (config.get_flag_name(Config::And).c_str(), "requires all selection to match.\n")
        (config.get_flag_name(Config::Or).c_str(), "allows any selection to match.\n")
        (config.get_flag_name(Config::Revision_script).c_str(), PO::value<string>(),
            "specify the script to be used to get revision information for the source files.\n")
        (config.get_flag_name(Config::Render_format).c_str(), po::value<string>(),
            "specifiy the output format of graph files for the render commands.\n")
       (config.get_flag_name(Config::Per_source).c_str(), "creates output files per source.\n")
       (config.get_flag_name(Config::Per_function).c_str(), "creates output files per function.\n")
       (config.get_flag_name(Config::Sort_line).c_str(), "sorts functions by line number.\n")
       (config.get_flag_name(Config::Sort_name).c_str(), "sorts functions by name.\n")
       (config.get_flag_name(Config::Show_external_inline).c_str(), "shows line coverage information for inlined source code.\n")
       (config.get_flag_name(Config::Hide_external_inline).c_str(), "hidess line coverage information for inlined source code.\n")
        ;

        po::options_description main_options;
        main_options.add(cmd_options).add(config_options).add(cmd_config);
        
        PO::positional_options_description p;
        p.add(Config::Selection.c_str(), -1);

        PO::variables_map vm;
        PO::store(PO::command_line_parser(ac, av).options(main_options).positional(p).run(), vm);
        PO::notify(vm);

        if (vm.count(Help.c_str())) 
        {
           print_help();
           cmd_help();
           cmd_config_help();
           return 1;
        }

        if (vm.count(Help_config.c_str()))
        {
           config_help();
           cmd_config_help();
           return 1;
        }

        if (vm.count(Version.c_str()))
        {
           cout << "\nTrucov Test Coverage Analysis Tool\n"
                << "Version " << VERSION << "\n\n";
           return 1;
        }

        // Get default config path.
        string config_path = getenv( "HOME" );
        config_path.append( "/.trucovrc" );
        ifstream config_stream;
        
        // If config file was specified.
        if (vm.count(config.get_flag_name(Config::Config_file).c_str())) 
        {
            config_stream.open(
                vm[config.get_flag_name(Config::Config_file).c_str()].as<string>().c_str());
            if (!config_stream) {
                cerr << "Config file doesn't exist.\n";
                return 1;
            }
        }
        else // Use default config file.
        {
            config_stream.open( config_path.c_str() );;
        }

        // Attempt to 
        if ( config_stream )
        {
           // Read the whole file into a string
           stringstream ss;
           ss << config_stream.rdbuf();
           string file = ss.str();       

           // Split the file content
           char_separator<char> sep(" \n\r");
           tokenizer<char_separator<char> > tok(file, sep);
           vector<string> args;
           copy(tok.begin(), tok.end(), back_inserter(args));
           
           // Parse the file and store the options
           po::store(
              po::command_line_parser(args).options(main_options).positional(p).run(), 
              vm);
        }

        bool res = config.initialize(vm, ac, av); 
        if (!res)
        {
           exit(0);
        }

        // Initialize command.
        Command & command = Command::get_instance(); 
        command.do_command(config.get_command());
    }
    catch(exception& e) 
    {
        cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
    catch(...) 
    {
        cerr << "Exception of unknown type!\n";
    }

    return 0;
}

void print_help()
{
    const string command_help =
"\nUsage: trucov [ command ] [ option... ] [ selection... ]\n\
\nCommand                         Description\n\
-------                         -----------\n\
status          Prints a coverage summary for each function to stdout.\n\n\
list            Print the name of functions to stdout.\n\n\
report          Creates or overwrites a coverage file for each source file in\n\
                the output directory\n\n\
dot             Outputs a single dot file. The dot file will show the coverage\n\
                control flow of all functions.\n\n\
dot_report      Creates or overwrites a dot file for each source file in the\n\
                output directory\n\n\
graph           Outputs a single graph file. The graph file shows the control\n\
                flow of all functions from all sources.\n\n\
graph_report    Creates or overwrites a graph file for each source file in\n\
                the output directory.\n\n\
all_report      Peforms report and render_report commands.\n\n"; 

    cout << command_help << flush;
}

void cmd_help()
{
    const string command_line_help =  
"Command Line Options:\n\n\
 -h [ --help ]     produce general help message.\n\n\
 --help-config     produce help for options.\n\n\
 -c [ --chdir ]    specify the directory where Trucov will run.\n\n\
 -o [ --output ]   specify the output directory where trucov should use.\n\n\
 --selection       specify the function(s) in all source files.\n\n\
 -v [ --version ]  prints out the working version number of Trucov.\n\n\
 -d [ --debug ]    printout debug and testing information.\n\n\
 --brief           only prints function summaries in coverage reports.\n\n\
 --only-missing    only outputs coverage information on functions with less\n\
                   than 100% coverage.\n\n\
 --show-fake       outputs coverage information on fake arcs and fake blocks.\n\n\
 --hide-fake       hides coverage information on fake arcs and fake blocks.\n\
                   [ default ]\n\n\
 --mangle          outputs the mangled names of functions.\n\n\
 --demangle        outputs the demangled names of functions.[ default ]\n\n\
 --partial-match   specify selection will be matched partially.[ default ]\n\n\
 --full-match      specify selection will be matched fully.\n\n\
 --signature-match specifies selection for functions shall be matched using the\n\
                   full function signature.\n\n\
 --config-file     specify the config file.\n\n";

    cout << command_line_help << flush;
}

void config_help()
{
  cout << "Config Options:\n\n"
       << " --default-command    the command used when no command is specified.\n\n"
       << " --default-selection  the selection used when no selection is specified.\n" << endl;
}

void cmd_config_help()
{
  cout << "Command Line + Config Options:\n\n"
       << " -b [ --builddir ]    specify the root directorie(s) of the GCNO and GCDA files.\n\n"
       << " -s [ --srcdir ]      specify the directorie(s) of the source files.\n\n"
       << " --per-source         report commands create coverage report files per source\n\n"
       << " --per-function       report commands create coverage report files per function\n\n" 
       << " --and                requires all selection to match.\n\n"
       << " --or                 allows any selection to match.\n\n"
       << " --revision-script    specify the script to be used to get revision information \n"
       << "                      for the source files.\n\n"
       << " --render-format      specifiy the output format of graph files for the render\n"
       << "                      commands. Value may be 'pdf' or 'svg', without quotes.\n" << endl;
}

// MAIN FUNCTION
int main(int ac, char* av[])
{
   boost_cmd_handler(ac, av);

   cerr << flush;
   cout << flush;
}




