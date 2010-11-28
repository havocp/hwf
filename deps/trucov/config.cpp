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
///  @file config.cpp
///
///  @brief
///  Implements the config class methods.
///////////////////////////////////////////////////////////////////////////////

// SYSTEM INCLUDES
#include <iostream>
#include <vector>

// LOCAL INCLUDES
#include "config.h"

// USING STATEMENTS

using std::map;
using std::string;
using std::pair;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;


// Initialize the config instance
Config* Config::config_instance = NULL;

// Initialize the string commands.
const string Config::Status = "status";
const string Config::List = "list";
const string Config::Report = "report";
const string Config::Dot = "dot";
const string Config::Dot_report = "dot_report";
const string Config::Graph = "graph";
const string Config::Graph_report = "graph_report";
const string Config::All_report = "all_report";

// Initialize the string options. 
const string Config::Selection = "selection";
const string Config::Source_directory = "srcdir";
const string Config::Build_directory = "builddir";
const string Config::Command = "command";

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Construct config instance and Initializes the config member variables. 
/// 
///////////////////////////////////////////////////////////////////////////////
Config::Config()
{
   m_bool_flag_name[Brief] = "brief";
   m_bool_flag_name[Debug] = "debug";
   m_bool_flag_name[Per_function] = "per-function";
   m_bool_flag_name[Per_source] = "per-source";
   m_bool_flag_name[Or] = "or";
   m_bool_flag_name[And] = "and";
   m_bool_flag_name[Only_missing] = "only-missing";
   m_bool_flag_name[Show_fake] = "show-fake";
   m_bool_flag_name[Hide_fake] = "hide-fake";
   m_bool_flag_name[Mangle] = "mangle";
   m_bool_flag_name[Demangle] = "demangle";
   m_bool_flag_name[Partial_match] = "partial-match";
   m_bool_flag_name[Full_match] = "full-match";
   m_bool_flag_name[Signature_match] = "signature-match";
   m_bool_flag_name[Sort_line] = "sort-line";
   m_bool_flag_name[Sort_name] = "sort-name";
   m_bool_flag_name[Show_external_inline] = "show-external-inline";
   m_bool_flag_name[Hide_external_inline] = "hide-external-inline";
   m_bool_flag_name[Secret_gui] = "secret-gui";

   m_string_flag_name[Render_format] = "render-format";
   m_string_flag_name[Revision_script] = "revision-script";
   m_string_flag_name[Output] = "output";
   m_string_flag_name[Working_directory] = "chdir";
   m_string_flag_name[Config_file] = "config-file";
   m_string_flag_name[Cache_file] = "cache-file";
   m_string_flag_name[Outfile] = "outfile";

   command = "status";
}

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns reference to config object.
///
/// @return Reference to config.
//////////////////////////////////////////////////////////////////////////////
Config & Config::get_instance()
{
    // Config instance has not been created
    if ( Config::config_instance == NULL )
    {
        // Create new instance
        config_instance = new Config();
    }

    // return the address of config instance
    return *config_instance;

} // End of Config::get_instance()

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// initialize the member variables
///
/// @param vm boost program options variable_map
/// @param ac argument count
/// @param av[] argument array
//////////////////////////////////////////////////////////////////////////////
bool Config::initialize(po::variables_map var_map, int ac, char* av[])
{
    Tru_utility* sys_utility = Tru_utility::get_instance();

    sys_utility->change_dir("");
    src_list.clear();
    build_list.clear();
    
    command = Status;
    if (ac > 1 && av[1][0] != '-')
    {
        command = string(av[1]);
    }

    string workingdir = ".";
    if ( var_map.count(get_flag_name(Working_directory)) )
    {
        // store chdir path
        workingdir = var_map[get_flag_name(Working_directory)].as<string>();

        // get absolute path of workingdir
        workingdir = sys_utility->get_abs_path(workingdir);
       
        if ( sys_utility->is_dir( workingdir ) )
        {
           // change directory
           sys_utility->change_dir(workingdir);
        }
        else
        {
           cerr << "ERROR: Directory " 
                << var_map[get_flag_name(Working_directory)].as<string>() 
                << " does not exists.\n";
           return false;
        }
    }
    else // Use default workingdir
    {
        workingdir = sys_utility->get_cwd();
    }
    m_string_flag_value[Working_directory] = workingdir;

    string rev_script = "";
    if ( var_map.count(get_flag_name(Revision_script)) )
    {
        // store revision-script path
        rev_script = var_map[get_flag_name(Revision_script)].as< string >();

        // get the absolute path and store in rev_script
        rev_script = sys_utility->get_abs_path_file(rev_script);
    
        if ( !fs::exists( rev_script ) )
        {
            cerr << "Warning: Revision script " << rev_script 
                 << " not found. Continuing without revision information." << endl;
            rev_script = "";
        }
    }
    m_string_flag_value[Revision_script] = rev_script;

    string outdir = "."; 
    string outfile;
    if ( var_map.count(get_flag_name(Output)) )
    {
       // store output dir in outdir
       outdir = var_map[get_flag_name(Output)].as<string>();
       
       // store absolute path 
       fs::path tmp_path = sys_utility->get_abs_path(outdir);
       outdir = tmp_path.string();

       if (command == Dot || command == Graph)
       {
          outdir = sys_utility->get_cwd();
          outfile = var_map[get_flag_name(Output)].as<string>();
          if (outfile[outfile.size() - 1] == '/')
          {
             cerr << "ERROR: " << command 
                  << " requires the output option to be a file." << endl;
             return false;
          }
       }
       else if( !sys_utility->is_dir(tmp_path.string()) )
       {
          // parent dir exists
          if (command != Status && command != List && !sys_utility->make_dir(outdir) )          
          {
             cerr << "Warning: Output directory could not be used. Defaulting to"
                  << " project directory." << endl;

             // assign cwd to outdir
             outdir = sys_utility->get_cwd();
          }
       }
    }
    else // Use default outdir
    {
       outdir = sys_utility->get_cwd();
    }
   
    m_string_flag_value[Outfile] = outfile; 

    Tru_utility* tru_utility = Tru_utility::get_instance();
    if( outdir[ outdir.length() - 1 ] != tru_utility->get_file_del() )
    {
       outdir += tru_utility->get_file_del();
    }
    m_string_flag_value[Output] = outdir;

    // input exists for builddir
    builddir.clear();
    if ( var_map.count(Build_directory) )
    {
       // store build dir path(s) in the buffer
       vector<string> buildbuf = var_map[Build_directory].as< vector<string> >();

       // iterator for the list
       vector<string>::iterator build_itr;

       // assigh beginning of the list to the iterator
       build_itr = buildbuf.begin();

       // go through the list
       while( build_itr != buildbuf.end() )
       {
          // get absolute path for each path
          *build_itr = sys_utility->get_abs_path( *build_itr );

          // the path is a directory path
          if ( sys_utility->is_dir( *build_itr ) )
          {
             // store in builddir list
             builddir.push_back( *build_itr );
          }
          // the path is not a directory
          else
          {
             // print error message
             cerr << "Warning: " << *build_itr << " is an invalid directory."
                  << " Continuing without the specified build directory." << endl;
          }

          //increment the iterator
          build_itr++;
       }
    }
    else // Use default buildir
    {
        builddir.push_back( sys_utility->get_cwd() );
    }

    // input exists for srcdir
    srcdir.clear();
    if ( var_map.count(Source_directory) )
    {
       // store direcotry path(s) to the buffer
       vector<string> srcbuf = var_map[Source_directory].as< vector<string> >();

       // iterator for the list
       vector<string>::iterator src_itr;

       // assign beginning of the list to the iterator
       src_itr = srcbuf.begin();

       // go through the list
       while( src_itr != srcbuf.end() )
       {
          // get absolute path 
          *src_itr = sys_utility->get_abs_path( *src_itr );

          // the path is a directory
          if ( sys_utility->is_dir( *src_itr ) )
          {
             // store in srcdir
             srcdir.push_back( *src_itr );
          } 
          else // the path is not a directory
          {
             // print error message
             cerr << "Warning: " << *src_itr << " is an invalid directory."
                  << " Continuing without the specified source directory." << endl;
          }

          // increment the iterator
          src_itr++;
       }
    } 
    else // Use default srcdir 
    {
       srcdir.push_back( sys_utility->get_cwd() );
    }
   
    m_bool_flag_value[Secret_gui] = var_map.count(get_flag_name(Secret_gui));  
    m_bool_flag_value[Debug] = var_map.count(get_flag_name(Debug));  
    m_bool_flag_value[Brief] = var_map.count(get_flag_name(Brief));
    m_bool_flag_value[Only_missing] = var_map.count(get_flag_name(Only_missing)); 
    m_bool_flag_value[Signature_match] = var_map.count(get_flag_name(Signature_match));
    
    m_bool_flag_value[And] = !var_map.count(get_flag_name(Or));
    m_bool_flag_value[And] = var_map.count(get_flag_name(And));
    m_bool_flag_value[Or] = !m_bool_flag_value[And];
 
    m_bool_flag_value[Show_fake] = !var_map.count(get_flag_name(Hide_fake)); 
    m_bool_flag_value[Show_fake] = var_map.count(get_flag_name(Show_fake));
    m_bool_flag_value[Hide_fake] = !m_bool_flag_value[Show_fake];

    m_bool_flag_value[Mangle] = !var_map.count(get_flag_name(Demangle)); 
    m_bool_flag_value[Mangle] = var_map.count(get_flag_name(Mangle)); 
    m_bool_flag_value[Demangle] = !m_bool_flag_value[Mangle]; 

    m_bool_flag_value[Full_match] = !var_map.count(get_flag_name(Partial_match));
    m_bool_flag_value[Full_match] = var_map.count(get_flag_name(Full_match));
    m_bool_flag_value[Partial_match] = !m_bool_flag_value[Full_match];

    m_bool_flag_value[Per_function] = !var_map.count(get_flag_name(Per_source));   
    m_bool_flag_value[Per_function] = var_map.count(get_flag_name(Per_function));   
    m_bool_flag_value[Per_source] = !m_bool_flag_value[Per_function]; 
   
    m_bool_flag_value[Sort_line] = var_map.count(get_flag_name(Sort_line));
    m_bool_flag_value[Sort_line] = !var_map.count(get_flag_name(Sort_name));
    m_bool_flag_value[Sort_name] = !m_bool_flag_value[Sort_line]; 

    m_bool_flag_value[Show_external_inline] = 
        !var_map.count(get_flag_name(Hide_external_inline));
    m_bool_flag_value[Show_external_inline] = 
        var_map.count(get_flag_name(Show_external_inline));
    m_bool_flag_value[Hide_external_inline] = !m_bool_flag_value[Show_external_inline]; 

    // input exists for selection
    if ( var_map.count(Selection) )
    {
        // store input in select_list
        select_list = var_map[Selection].as< vector<string> >();
    }
    
    // Use render format specified.
    if ( var_map.count(get_flag_name(Render_format)) )
    {
	     string selected_format = 
            var_map[get_flag_name(Render_format)].as<string>();	
   
        if ( selected_format != "svg" && selected_format != "pdf" )
        {
            cerr << "Warning: Render format '" << selected_format
                 << "' is an invalid format. Formats are 'svg' or 'pdf'."
                 << " Defaulting to 'svg'." << endl;
            selected_format = "svg";      
        }
 
        m_string_flag_value[Render_format] = selected_format;
    }
    else // Use render defualt.
    {
        m_string_flag_value[Render_format] = "svg";
    }

#ifdef DEBUGFLAG
   if ( get_flag_value(Debug) )
   {
      // print out source directories for DEBUG
      cout << "Source Directories = { ";
      for (int i = 0; i < (int)srcdir.size(); i++)
         cout << srcdir[i] << " ";
      cout << "};\n";

      // print out build directories for DEBUG
      cout << "Build Directories = { ";
      for (int i = 0; i < (int)builddir.size(); i++)
         cout << builddir[i] << " ";
      cout << "};\n";

      // print out output directories for DEBUG
      cout << "Output Directory = { " << outdir << " }\n";
      
      cout << "This is " << command << endl;
   }
#endif

    // get GCNO and GCDA files
    collect_build_files();

    // get source files
    collect_src_files();

    // get selection from 
    collect_selection(ac, av);

    return true;
}

bool Config::get_flag_value(Bool_flag flag)
{
   return m_bool_flag_value[flag];
}

string Config::get_flag_value(String_flag flag)
{
   return m_string_flag_value[flag];
}

string Config::get_flag_name(Bool_flag flag)
{
   return m_bool_flag_name[flag];
}

string Config::get_flag_name(String_flag flag)
{
   return m_string_flag_name[flag];
}

/// @brief
/// recursively goes through the file system hierarchy and collect GCNO and GCDA files 
///
/// @return void
void Config::collect_build_files()
{
   // get Tru_utility instance
   Tru_utility* sys_utility = Tru_utility::get_instance();

   // create iterator 
   vector<string>::iterator main_itr;

   //temporary container for gcno files
   vector<string> tmp_gcno;

   //temporary container for gcda files
   vector<string> tmp_gcda;

   // builddir is not empty
   while (!(builddir.empty()))
   {
      main_itr = builddir.end();
   
      // seek the last item
      main_itr--;

      // store in path
      string path = *main_itr;

      // pop the last item from the list
      builddir.pop_back();

      // read the dir
      vector<string> dir_contents = sys_utility->read_dir( path );

      // create temp iterator
      vector<string>::iterator tmp_itr = dir_contents.begin();

      // iterator is not pointing to the end
      while ( tmp_itr != dir_contents.end() )
      {
         // file that the iterator is pointing at is a GCNO file
         if ( is_gcno(*tmp_itr) )
         {
             // store in the GCNO list
             tmp_gcno.push_back(*tmp_itr);
         }

         // file that the iterator is pointing at is a GCDA file
         else if ( is_gcda(*tmp_itr) )
         {
             // store in the GCDA list
             tmp_gcda.push_back(*tmp_itr);
         }

         // it is a sub-directory
         else if ( sys_utility->is_dir(*tmp_itr) )
         {
             // store in the builddir list for searching
             builddir.push_back(*tmp_itr);
         }
         // increment the iterator
         tmp_itr++;
      }     
   }// end while

   // match tmp_gcno with tmp_gcda
   // create an iterator for tmp_gcno list 
   vector<string>::iterator g_itr = tmp_gcno.begin();

   // iterator is not pointing at the end
   while ( g_itr != tmp_gcno.end() )
   {
      // copy GCNO file path in gcno_str
      string gcno_str = *g_itr;

      // remove the "gcno" extension from the string
      gcno_str = gcno_str.erase( (gcno_str.size() - 4), 4);
     
      // create an iterator for tmp_gcda list
      vector<string>::iterator d_itr = tmp_gcda.begin();

      // iterator is not pointing at the end
      while ( d_itr != tmp_gcda.end() )
      {
         // copy GCDA file path in gcda_str
         string gcda_str = *d_itr;
         
         // remove the "gcda" extension from the string
         gcda_str = gcda_str.erase( (gcda_str.size() - 4), 4);

         // gcda_str and gcda_string are identical
         if ( gcda_str.compare(gcno_str) == 0 )
         {
             // make a pair of original string and store in the build_list
             build_list.push_back(make_pair(*g_itr, *d_itr));

             // stop searching
             break;
         }
         
         // increment the inner iterator
         d_itr++;
      }

      // no match is found and make a pair with empty string
      build_list.push_back(make_pair(*g_itr, ""));
 
      // increment outter iterator
      g_itr++;
   }

   // This part will be deleted in future releases
   // 
   // DO NOT DELETE THIS COMMENT
   // This is simply removing the pair with empty string from the list
   ////////////////////////////////////////////////////////////////////

   // create a temp_list
   vector< pair<string, string> > tmp_build;

   // empty the list
   tmp_build.empty();

   // declare a variable
   pair<string, string> tmp_pair;

   // get iterator pointed to the beginning of the list
   vector< pair<string, string> >::iterator build_list_itr = build_list.begin();

   // iterator is not at the end
   while (build_list_itr != build_list.end())
   {
      // copy file pair from the list
      tmp_pair = *build_list_itr;

      // second part of the file pair is not an empty string
      if (tmp_pair.second.compare("") != 0)
      {
         // add to the list
         tmp_build.push_back(tmp_pair);
      }

      // increment the iterator
      build_list_itr++;
   }

   // empty the build_list
   build_list.empty();

   // assign tmp list to the build_list
   build_list = tmp_build;
   ////////////////////////////////////////////////////////////////////
}

/// @brief
/// recursively goes through the file system hierarchy and collect source files 
///
/// @return void
void Config::collect_src_files()
{
   // get Tru_utility instance
   Tru_utility* sys_utility = Tru_utility::get_instance();

   // create an iterator for the srcdir list
   vector<string>::iterator main_itr;

   // srcdir list is not empty
   while (!(srcdir.empty()))
   {
      // seek to the end
      main_itr = srcdir.end();

      // seek to the last item
      main_itr--;

      // copy path from the list
      string path = *main_itr;

      // remove the path from the list
      srcdir.pop_back();

      // read the dir
      vector<string> dir_contents = sys_utility->read_dir( path );

      // seek the beginning of the dir_content
      vector<string>::iterator tmp_itr = dir_contents.begin();

      // iterator not at the end
      while ( tmp_itr != dir_contents.end() )
      {
         // If C or C++ file
         if ( is_source_file(*tmp_itr) )
         {
             // store to the list
             src_list.push_back(*tmp_itr);
         
         } // If directory 
         else if ( sys_utility->is_dir(*tmp_itr) )
         {
             // store to the srcdir list
             srcdir.push_back(*tmp_itr);
         }

         // increment the iterator
         tmp_itr++;
      }
   }//end while
}

/// @brief
/// geather file names or function names from the command line and store in the selection list
///
/// @param ac command line argument count
/// @param av[] argument array
///
/// @return void 
///
/// @remark this function is not tested
void Config::collect_selection(int ac, char* av[])
{
   string tmp_str;
   for(int arg_idx = 1; arg_idx < ac; arg_idx++)
   {
      tmp_str = "";
      if (av[arg_idx][0] == '"')
      {
         tmp_str = av[arg_idx];
         tmp_str.erase(0, 1);
         tmp_str.erase(tmp_str.size() - 1, 1);
         select_list.push_back(tmp_str);
         cout << tmp_str << "\n";
      }
   }

   // If command was specified, then remove it from selection.
   // there are more than one argument
   if (ac > 1)
   {
      // the second argument is not an option flag
      if (av[1][0] != '-')
      {
         // it must be a command and remove it from the list
         select_list.erase(select_list.begin());
      } 
   }
   
}


/// @brief
/// getter function for src_list
///
/// @return vector< string >
const vector<string> & Config::get_source_files()
{
    return src_list;
}

/// @brief
/// getter function for build_list
///
/// @return vector< pair <string, string> >
vector< pair<string, string> > Config::get_build_files()
{
   return build_list;
}

/// @brief
/// getter function for selection
///
/// @return vector< string >
const vector<string> & Config::get_selection()
{
   return select_list;
}

/// @brief
/// getter function for command 
///
/// @return const string
const string & Config::get_command() const
{
    return command;
}

/// @brief
/// Determine whether input is a GCNO file or not 
///
/// @param file_name file path
///
/// @return bool
const bool Config::is_gcno(string file_name)
{
   // chop up file name and path leave only extension
   file_name.erase(0, file_name.length() - 5);

   return (file_name == ".gcno");
}

/// @brief
/// Determine whether input is a GCDA file or not 
///
/// @param file_name file path
///
/// @return bool
const bool Config::is_gcda(string file_name)
{
   // chop up file name and path leave only extension
   file_name.erase(0, file_name.length() - 5);

   return (file_name == ".gcda");   
}

const bool Config::is_source_file(const string & file)
{
   string extension_types[] = 
      {"c", "cc", "cpp", "h", "hh", "hpp", "m", "mm", ""};

   unsigned int dot_count = count(file.begin(), file.end(), '.');
   if ((dot_count == 0) || (dot_count == 1 && file[0] == '.'))
   {
      return false;
   }

   unsigned int extension_start = file.find_last_of('.') + 1;
   if (extension_start < file.size())
   {
      string extension = file.substr(extension_start, file.size() - extension_start + 1);

      for (int i = 0; extension_types[i] != ""; i++)
      {
         if (extension == extension_types[i])
         {
            return true;
         }
      }
   }

   return false;
}

