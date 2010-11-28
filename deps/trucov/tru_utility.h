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
///  @file tru_utility.h 
///
///  @brief
///  Defines the utility functions for Trucov.
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

#ifndef TRU_UTILITY_H
#define TRU_UTILITY_H

// SYSTEM INCLUDES

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <map>
#include <algorithm>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/progress.hpp>
#include <boost/unordered_map.hpp>

#include "source_file.h"

// FOWARD REFERECNES

namespace fs = boost::filesystem;

///////////////////////////////////////////////////////////////////////////////
///  @class Tru_Utility
///
///  @brief
///  Utility functions for trucov
///////////////////////////////////////////////////////////////////////////////
class Tru_utility
{
   public:
      /// @brief
      /// Cleans up after the Tru_Utility object
      ~Tru_utility();

      /// @brief
      /// Change current working directory. Returns true when success, false otherwise.
      ///
      /// @param absolute or relative directory path.
      ///
      /// @return boolean
      bool change_dir( std::string path ); 

      /// @brief
      /// getter for m_curr_dir
      ///
      /// @return
      /// currently working directory path
      std::string get_cwd();

      /// @brief
      /// read contents of directory
      ///
      /// @param path absolute directory path
      ///
      /// @return
      /// a list of strings containing directory contents
      std::vector<std::string> read_dir( std::string path );

      /// @brief
      /// determine input path is directory or not
      ///
      /// @param path directory path
      ///
      /// @return
      /// boolean
      bool is_dir(std::string path); 

      /// @brief
      /// Create a directory
      ///
      /// @param path absolute directory path
      ///
      /// @return
      /// boolean
      bool make_dir(std::string path);

      /// @brief
      /// find the relative path to the current working directory
      ///
      /// @param path absolute directory path
      ///
      /// @return
      /// relative path as a string
      std::string get_rel_path(std::string path);

      /// @brief
      /// find the absolute directory path of the input
      ///
      /// @param raw_path directory path
      ///
      /// @return
      /// an absolute path as a string
      std::string get_abs_path(std::string raw_path);

      /// @brief
      /// find the absolute file path of the input
      ///
      /// @param raw_path file path
      ///
      /// @return
      /// an absolute path as a string
      std::string get_abs_path_file(std::string raw_path);

      /// @brief
      /// get the pointer of tru_utility instance
      ///
      /// @return
      /// a pointer to the tru_utility instance
      ///
      /// @remark it doesn't gurantee that the directory or file exists
      static Tru_utility* get_instance();

      /// @brief
      /// 
      ///
      /// @param const  vestor<string> &srcFiles : dir list of source file 
      /// @param const  string &srcGCNO : path from GCNO
      ///
      /// @return 
      /// const boolean
      const bool is_within_project( const std::vector<std::string> &srcFiles,
                                    const std::string &srcGCNO ) const;

      /// @brief
      /// 
      /// @param const  vestor<string> &srcFiles : dir list of source file 
      /// @param const  string &srcGCNO : path from GCNO
      /// @param string & source_path
      ///
      /// @return 
      /// const boolean
      const bool is_within_project( const std::vector<std::string> &srcFiles,  
                                    const std::string & srcGCNO,
                                    std::string & source_path ) const;

      /// @brief
      /// 
      /// @param const  string & pathname : path from GCNO
      ///
      /// @return 
      /// const string
      const std::string get_filename( const std::string & pathname ) const;

      /// @brief
      /// 
      /// @param const  string & filename
      ///
      /// @return 
      /// const string
      const std::string strip_extension( const std::string & filename ) const;

      /// @brief
      /// 
      /// @param const  string & pathname
      ///
      /// @return 
      /// const string
      const std::string get_basename( const std::string & pathname ) const;

      /// @brief
      /// 
      /// @param string str
      /// @param const  string & type
      ///
      /// @return 
      /// string
      std::string genSelcovFileName(std::string str, const std::string & type);

      /// @brief
      /// 
      /// @param string source_name
      /// @param const string & extension
      ///
      /// @return 
      /// string
      std::string create_file_name( 
         std::string source_name, 
         const std::string & extension );

      /// @brief 
      /// 
      /// @param string command_name
      /// @param string command_argument
      ///
      /// @return 
      /// string
      std::string execute_pipe(
         std::string command_name, 
         std::string command_argument);

      /// @brief 
      /// 
      /// @param string command_name
      /// @param string command_argument
      ///
      /// @return 
      /// string
      char get_file_del();

      /// @brief
      ///
      /// @param signature The function signature
      std::string escape_function_signature(const std::string & signature);

      /// @brief
      /// Populates a vector with witht the sorted records.
      void sort_records(
         std::vector<Record *> & list, 
         std::map<Source_file::Source_key, Record> & records,
         bool sort_line);

      /// @brief
      /// Removes the ".."'s and "." from a pathname.
      ///
      /// @param filepath The raw filepath
      ///
      /// @return The cleaned path as an absolute path. 
      std::string clean_path( const std::string & filepath );

   private:

      /// @brief Tru_utility constructor
      Tru_utility();

      // a pointer for Tru_utility instance
      static Tru_utility* tru_utility_ptr;

      // current working directory
      fs::path m_curr_dir;

      // deliminator 
      char m_del;

      struct Cache_value
      {
         /// Negative cache value
         Cache_value( ) :
            within_project(false)
         { }

         /// Positive cache value
         explicit Cache_value( const std::string& source_path ) :
            within_project(true),
            source_path(source_path)
         { }

         bool within_project;

         std::string source_path;
      };
      typedef boost::unordered_map<std::string,Cache_value> Cache;

      /// cache lookup for is_within_project
      mutable Cache m_cache;
};
#endif
