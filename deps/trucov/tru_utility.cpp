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
///  @file tru_utility.cpp
///
///  @brief
///  Implements the tru_utility class.
///////////////////////////////////////////////////////////////////////////////

#include "tru_utility.h"

#include <boost/tuple/tuple.hpp>

using std::sort;
using std::map;
using std::vector;
using std::string;

// initialize Tru_utility instance to NULL
Tru_utility* Tru_utility::tru_utility_ptr = NULL;

/// @brief Tru_utility constructor
Tru_utility::Tru_utility() 
{
      // initialize current working directory
      m_curr_dir = fs::current_path<fs::path>();

   // native os is windows
   #ifdef BOOST_WINDOWS_API
      // initialize deliminator
      m_del = '/\\';
   #endif

   // native os is linux
   #ifdef BOOST_POSIX_API
      // initialize deliminator
      m_del = '/';
   #endif
}

Tru_utility::~Tru_utility()
{

}

bool Tru_utility::change_dir(string in_path) 
{
   if (in_path.size() == 0)
   {
      string s = fs::system_complete(".").directory_string();
      s.erase(s.end() - 1);
      m_curr_dir = fs::path(s); 
      return true;
   }

   fs::path tmp_path( in_path );
   // input is an absolute path
   if ( tmp_path.is_complete() )
   {
      // input is a directory
      if ( fs::is_directory( tmp_path ) && fs::exists( tmp_path ) )
      {
         m_curr_dir = tmp_path;
         return true;
      }
      // input is not a directory
      return false;
   }
   else
   {
      fs::path tmp_cwd( get_cwd() );
      tmp_cwd /= tmp_path;
      
      if (fs::is_directory( tmp_cwd ) && fs::exists( tmp_cwd ) )
      {
         m_curr_dir = tmp_cwd;
         return true;
      }
      return false;
   }

}

string Tru_utility::get_cwd() 
{
   // assign m_curr_dir to tmp_str
   string tmp_str =  m_curr_dir.string();
   if ( tmp_str[ tmp_str.size() - 1 ] != m_del )
   {
      tmp_str += m_del;
   }
   //tmp_str.erase( tmp_str.size() - 1 );
   return clean_path(tmp_str);
}

vector<string> Tru_utility::read_dir( string path )
{
   // create a string vector
   vector<string> result;

   // end of the dir content list
   fs::directory_iterator end_itr;

   // loop through the directory contents
   for ( fs::directory_iterator dir_itr( path ) ; dir_itr != end_itr; ++dir_itr )
   {
      // add to the list
      result.push_back( dir_itr->path().file_string() );
   }
 
   // return the list
   return result;
}

bool Tru_utility::is_dir( string path ) 
{
   if( fs::exists( path ) )
   {
      // return true if input is a directory, false otherwise
      return fs::is_directory( path );
   }
   return false;
}

bool Tru_utility::make_dir( string path )
{
   fs::path tmp_path( path );

   // parent path exists
   if ( fs::exists( tmp_path.remove_leaf() ) )
   {
      // create a directory
      fs::create_directory( path );
      return true;
   }

   // parent path does not exist
   return false;
}

string Tru_utility::get_abs_path( string raw_path )
{
   // create a path
   fs::path r_path( raw_path );

   // get current working directory
   fs::path tmp_path( get_cwd() );
  
   // concatinate realtive path to the current working dir
   tmp_path /= r_path;

   // input is an absolute path
   if (r_path.is_complete())
   { 
      // return input
      return clean_path(r_path.string());
   }
   // new path is an absolute and the path exist
   else if (tmp_path.is_complete())
   {
      // return concatinated path
      return clean_path(tmp_path.string());
   }
   // return empty string
   return "";
}

string Tru_utility::get_rel_path(string path)
{
   fs::path tmp_path( path );
   // input exists
   if ( fs::exists( tmp_path ) )
   {
      //cout << "input string: " << path << "\n";
      // temporary string
      string tmp_str;

      // assign current working dir to tmp_str
      tmp_str = get_cwd();

      //cout << "cwd: " << tmp_str << "\n";
      // cwd path is the preceding part of input
      if(path.find(tmp_str) == 0)
      {
         // remove the cwd path
         path.erase(0, tmp_str.length() );

         // input is the currnet working dir
         if (path.length() == 0)
         {
            return "";
         }
         //cout << "result: " << path << "\n\n";
         //return relative path
         if ( (path[0] == '.') && (path[1] == m_del) )
         {
            path.erase(0, 2);
         }
         return path;
      }

      // cwd is not the preceding part of input
      else
      {
         string result = "";
 
         // chop the cwd path up to the sharing path
         while (path.find(tmp_str) != 0)
         {
            tmp_str.erase(tmp_str.length() - 1);
            while (tmp_str[tmp_str.length() - 1] != m_del)
            {
               tmp_str.erase(tmp_str.length() - 1);
            }

            // add ".." for each dir 
            result = result + ".." + m_del;
         }

         // remove the sharing path
         path.erase( 0, tmp_str.length() );

         // concatinate ../.. with the rest
         result = result + path;

         if ( (result[0] == '.') && (result[1] == m_del) )
         {
            result.erase(0, 2);
         }
         //cout << "result: " << result << "\n\n";
         return result;
      }
   }
   // input does not exist
   return path;
}

// This function gotta go away talk to Matt
string Tru_utility::get_abs_path_file(string raw_path)
{
   string dir_path = get_filename(raw_path);
   raw_path.erase(raw_path.size() - dir_path.size(), raw_path.size());
   raw_path = get_abs_path(raw_path) + dir_path;
   return clean_path(raw_path);
}

Tru_utility* Tru_utility::get_instance()
{
   //if Tru_Utility instance's not been created
   if ( Tru_utility::tru_utility_ptr == NULL )
   {
      //create Tru_Utility
      tru_utility_ptr = new Tru_utility();
   }
   //return pointer of Tru_Utility instance
   return tru_utility_ptr;
}

const bool Tru_utility::is_within_project( const vector<string> &srcFiles, const string &srcGCNO ) const
{
   string temp;

   return is_within_project( srcFiles, srcGCNO, temp );
}

const bool Tru_utility::is_within_project( 
   const vector<string> &srcFiles, 
   const string & srcGCNO,
   string & source_path ) const
{
   Cache::const_iterator found = m_cache.find(srcGCNO);

   if( found == m_cache.cend() )
   {
      const string temp = get_filename( srcGCNO );
      for ( unsigned i = 0; i < srcFiles.size(); ++i )
      {
         if ( temp == get_filename( srcFiles[i] ) )
         {
            bool success;
            boost::tie(found,success) = m_cache.insert( std::make_pair( srcGCNO, Cache_value(srcFiles[i]) ));
            assert(success);
            break;
         }
      }

      if( found == m_cache.cend() )
      {
         bool success;
         boost::tie(found, success ) = m_cache.insert( std::make_pair( srcGCNO, Cache_value() ) );
         assert( success );
      }
   }
   assert( found != m_cache.end() );

   source_path = found->second.source_path;
   return found->second.within_project;
}

const string Tru_utility::get_filename( const string & pathname ) const
{
   string temp;
   size_t found;

   temp = pathname;
   found = temp.find_last_of("/\\");

   if ( found == 0 )
   {
      return temp;
   }
   else
   {
      return temp.substr( found + 1 );
   }
}

const string Tru_utility::strip_extension( const string & filename ) const
{
   string temp;
   size_t found;

   temp = filename;
   found = temp.find_last_of(".");

   if ( found == 0 )
   {
      return temp;
   }
   else
   {
      return temp.substr( 0, found );
   }
}

const string Tru_utility::get_basename( const string & pathname ) const
{
   string temp = pathname;

   temp = get_filename( temp );
   temp = strip_extension( temp );

   return temp;
}

string Tru_utility::genSelcovFileName(string str, const string & type)
{
   string::iterator it;

   // Find last '/' character
   for (it = str.end() - 1; *it != '/'; it--)
   {
      // void
   }
   str.erase(str.begin(), it + 1);

   // Find last '.' character
   for (it = str.end() - 1; *it != '.'; it--)
   {
      // void
   }
   str.erase(it, str.end());
   // add .dot to end
   str.append(type);

   return str;
}

string Tru_utility::create_file_name( string source_name, const string & extension )
{
   // Instantiate utility class pointer
   Tru_utility * ptr_utility = Tru_utility::get_instance();

   // Get relative path and file name
   string file_name = ptr_utility->get_rel_path( source_name );
   
   if (file_name.size() > 2 && file_name.substr(0, 3) == "../")
   {
      file_name = "_" + file_name;
   }

   // Find and replace all instances of directory separators with "##"
   size_t found = file_name.find('/');
   while ( found != string::npos )
   {
      file_name.replace( found, 1, "##" );
      found = file_name.find('/');
   }

   // Append .trucov file extension
   file_name += extension;  
   return file_name;
}

string Tru_utility::execute_pipe(string command_name, string command_argument)
{
   char input[5000];
   string rec_name = "";
   command_name.push_back(' ');
   FILE *fp = popen( (command_name + command_argument).c_str(), "r" );

   if (fp != NULL)
   {
      char * buf = fgets( input, sizeof( input ), fp );

      rec_name = input;

      unsigned int found = rec_name.find( "\n" );
      if ( found != string::npos )
      {
          rec_name.resize( found );
      }

      pclose(fp);
   }
   return rec_name;
}

char Tru_utility::get_file_del()
{
   return m_del;
}

string Tru_utility::escape_function_signature(const string & signature)
{
   string res;
   for (int i = 0; i < signature.size(); i++)
   {
      if (signature[i] == '(' || signature[i] == ')' || signature[i] == '&'
         || signature[i] == '*' || signature[i] == ',' || signature[i] == ' '
         || signature[i] == '<' || signature[i] == '>')
      {
         res.push_back('\\');
      }
      res.push_back(signature[i]);
   }
   return res;
}

void Tru_utility::sort_records(
   vector<Record *> & list,
   map<Source_file::Source_key, Record> & records,
   bool sort_line)
{
      list.clear();
      for( map<Source_file::Source_key, Record>::iterator i = records.begin();
           i != records.end();
           ++i )
      {
         list.push_back(&i->second);
      }

      // Sort by lines
      if (sort_line)
      {
         sort( list.begin(), list.end(), record_line_lessthan );
      }
      else // Sort by names
      {
         sort( list.begin(), list.end(), record_name_lessthan );
      }
}

string Tru_utility::clean_path( const string & filepath )
{
   string complete_path = fs::system_complete( filepath ).string();

   // Remove internal "."'s
   size_t pos = complete_path.find("/./");
   while ( pos != string::npos )
   {
      complete_path.replace(pos, 3, "/");
      pos = complete_path.find("/./");
   }

   // Remove internal ".."'s
   pos = complete_path.find("/../");
   while ( pos != string::npos )
   {
      size_t parent = complete_path.find_last_of("/", pos - 1);
      pos += 4;
      assert (parent != string::npos);
      complete_path.replace(parent, pos - parent, "/");
   
      pos = complete_path.find("/../");
   }

   // Remove end "." 
   if (complete_path.size() > 1 
       && complete_path.substr(complete_path.size() - 2, complete_path.size() - 1) == "/.")
   {
      complete_path.erase( complete_path.size() - 1);
   }

   // Remove end ".."
   if (complete_path.size() > 2
       && complete_path.substr(complete_path.size() - 3, complete_path.size() - 1) == "/..")
   {
      size_t parent = complete_path.find_last_of("/", complete_path.size() - 4);
      assert (parent != string::npos);
      complete_path.erase( parent + 1, complete_path.size() - 1 - parent);
   }

   return complete_path;
}

