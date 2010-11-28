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
///  @file config.h 
///
///  @brief
///  Defines the Configuration for Trucov.
/// 
///
///  @remarks
///  
///  
///
///  Requirements Specification:
///     < http://code.google.com/p/trucov/wiki/SRS > 
///
///  Design Description: 
///     < http://code.google.com/p/trucov/wiki/TrucovDesignDocumentation >
///////////////////////////////////////////////////////////////////////////////

#ifndef CONFIG_H
#define CONFIG_H

// SYSTEM INLUEDES

#include <iostream>
#include <string>
#include <vector>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

// LOCAL INCLUDES

#include "tru_utility.h"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

///////////////////////////////////////////////////////////////////////////////
///  @class Config
///
///  @brief
///  Specify the configuration for trucov.
///
///  @remarks
///  Class takes a variable map, argument count and argument array when
///  constructed.
///////////////////////////////////////////////////////////////////////////////
class Config
{
public:

// PUBLIC CONSTANTS

   /// The string of the status command.
   static const std::string Status;
   /// The string of the list command.
   static const std::string List;
   /// The string of the report command.
   static const std::string Report;
   /// The string of the dot command.
   static const std::string Dot;
   /// The string of the dot report command.
   static const std::string Dot_report;
   /// The string of the graph command.
   static const std::string Graph;
   /// The string of the graph_report command.
   static const std::string Graph_report;
   /// The string of the all_report command.
   static const std::string All_report;

   static const std::string Selection;
   /// The string of the source directory option.
   static const std::string Source_directory;
   /// The string of the build option.
   static const std::string Build_directory;
   static const std::string Command;
  
   enum Bool_flag
   {
      Debug,
      Per_function,
      Per_source,
      Brief,
      Or,
      And,
      Only_missing,
      Show_fake,
      Hide_fake,
      Mangle,
      Demangle,
      Partial_match,
      Full_match,
      Signature_match,
      Sort_line,
      Sort_name,  
      Show_external_inline,
      Hide_external_inline,
      Secret_gui 
   };

   enum String_flag
   {
      Render_format,
      Revision_script,
      Output,
      Working_directory,
      Config_file,
      Cache_file,
      Outfile  
   };

// PUBLIC METHODS

      /// @brief
      /// Returns the single instance of this singleton.
      ///
      /// @remarks
      /// Returns a reference to the singleton Config object.
      static Config & get_instance();

      /// @brief
      /// initialize the member variables
      ///
      /// @param vm boost program options variable_map
      /// @param ac argument count
      /// @param av[] argument array
      bool initialize(po::variables_map vm, int ac, char* av[]);

      bool get_flag_value(Bool_flag flag);
      std::string get_flag_value(String_flag flag);

      std::string get_flag_name(Bool_flag flag);
      std::string get_flag_name(String_flag flag);

      /// @brief
      /// Returns a list of ".c" or ".cpp" files from the src_dir.
      const std::vector<std::string> & get_source_files();

      /// @brief
      /// Returns a list of ".gcno" and ".gcda" files from the build_dir.
      std::vector< std::pair<std::string, std::string> > get_build_files();

      /// @brief
      /// Returns a list of selection.
      const std::vector<std::string> & get_selection();

      /// @brief
      /// Returns command that is used in the command line.
      const std::string & get_command() const;

  private:

// PRIVATE MEMBERS

      // The command specified.
      std::string command;

      // Pointer to the config instance
      static Config* config_instance;

      std::map<Bool_flag, bool> m_bool_flag_value; 
      std::map<Bool_flag, std::string> m_bool_flag_name; 

      std::map<String_flag, std::string> m_string_flag_value; 
      std::map<String_flag, std::string> m_string_flag_name; 

      // List of source directorie(s)
      std::vector<std::string> srcdir;

      // List of build directorie(s)
      std::vector<std::string> builddir;

      // List of source file(s)
      std::vector<std::string> src_list;

      // List of GCNO and GCDA fiels
      std::vector< std::pair<std::string, std::string> > build_list;

      // List of selection.
      std::vector<std::string> select_list;

      // Specifies default selection
      std::vector<std::string> default_select;

      /// @brief 
      /// config constructor.
      Config();

      /// @brief
      /// collects GCDA and GCNO files from the build dir
      void collect_build_files();

      /// @brief
      /// collects source files from the src_dir
      void collect_src_files();

      /// @brief
      /// collects selection files name or function name from the command line
      ///
      /// @param ac argument count
      /// @param av[] argument array
      ///
      /// @remark this function is not in use
      void collect_selection(int ac, char* av[]);

      /// @brief
      /// Determines the input path is a GCNO file or not
      ///
      /// @param string file path.
      const bool is_gcno(std::string);

      /// @brief
      /// Determines the input path is a GCDA file or not
      ///
      /// @param string file path.
      const bool is_gcda(std::string);

      /// @brief
      /// Determines if a file is a C or C++ file.
      ///
      /// @param file The filename. 
      const bool is_source_file(const std::string & file);
};

#endif
