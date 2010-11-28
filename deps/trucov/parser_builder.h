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
///  @file parser_builder.h 
///
///  @brief
///  Defines the Parser_builder class. 
///////////////////////////////////////////////////////////////////////////////
#ifndef PARSER_BUILDER_H
#define PARSER_BUILDER_H

// SYSTEM INCLUDES

#include <string>
#include <map>

#include <boost/noncopyable.hpp>

// LOCAL INCLUDES

#include "record.h"
#include "selector.h"
#include "revision_script_manager.h"
#include "source_file.h"

/// @brief
/// Builds the data structure of a map of files for the parser class.
class Parser_builder : boost::noncopyable
{
public:

// PUBLIC METHODS

   /// @brief
   /// Initializes the Parser_builder and stores a reference to the internal
   /// data of the Parser class.
   Parser_builder(
      std::map<std::string, Source_file> & source_files,
      const std::string & revision_script_path,
      std::vector<std::string> found_source_files );

   /// @brief
   /// Free memory used for demangling.
   ~Parser_builder( );

   /// @brief 
   /// Will add or merge a record into the m_records data in the correct
   /// source file.
   ///
   /// @param rIdent The record unique indentifier.
   /// @param rChecksum The record unique checksum.
   /// @param rSource The record source file name.
   /// @param rName The record source name.
   /// @param m_name_demangled The demangled name of the record source name.
   void store_record(
      unsigned int rIdent, 
      unsigned int rChecksum,
      const std::string & rSource,
      const std::string & rName,
      unsigned int rLineno );

   /// @brief
   /// Will add or merge the b_flags into the last record touched.
   /// 
   /// @param bLength The length of the block record.
   /// @param bFlags The block flags.
   /// @param bIteration The current iteration of this block
   void store_blocks( 
      unsigned int bLength,
      unsigned int bFlags,
      unsigned int bIteration ); 

   /// @brief
   /// Stores outgoing arc data to the last record added to a specified block.
   ///
   /// @param aBlockno The source blocks unique indentifier for the last
   /// record.
   /// @param aDestBlock The destination blocks unique indentifier for the
   /// last record.
   /// @param aFlags The arc flags.
   void store_arcs(
      unsigned int aBlockno,
      unsigned int aDestBlock,
      unsigned int aFlags );

   /// @brief
   /// Stores the lineno associated with a block
   ///
   /// @param lBlockno The indentifier of the block containing the lineno
   /// @param lLineno The line number.
   /// @param lName The source file of this line.
   void store_line_number( 
      unsigned int lBlockno, 
      unsigned int lLineno,
      const std::string & lName );

   /// @brief
   /// Stores the arc count in the last record added.
   ///
   /// @param aCount The arc count.
   void store_count( 
      unsigned int rIdent,
      unsigned int rChecksum,
      uint64_t aCount );

   /// @brief
   /// Assign arcs from cache to data structure.   
   void assign_arcs();

private:

// PRIVATE METHODS

   /// @brief
   /// Caches the last record stored / merged.
   ///
   /// @param last_record_ident The unique indentifier of the last record 
   /// added.
   ///
   /// @remarks
   /// This function exists to hide the fact the m_last_record_stores is an
   /// iterator.
   void set_last_record( 
      std::map<Source_file::Source_key, Record> & records,
      Source_file::Source_key last_record_key )
   {
      m_last_record_stored = records.find( last_record_key );
   } 

   /// @brief
   /// Returns the last record stored.
   ///
   /// @return Reference to the last Record stored / merged.
   ///
   /// @remarks
   /// This function exists to hide the fact that m_last_record_stored is
   /// an iterator. By not having the code use the member field directory,
   /// we help minimize the chance that the code will incorrectly modify the
   /// cache pointer. 
   Record & get_last_record()
   {
      return m_last_record_stored->second;
   }

// PRIVATE MEMBERS

   /// A cache of the last record added. Thus we don't have to keep looking 
   /// it up.
   std::map<Source_file::Source_key, Record>::iterator m_last_record_stored;

   /// A reference to the internal source file map in the Parser class.
   std::map<std::string, Source_file> & m_source_files;

   /// A list of all the source files found in the project directory.
   std::vector<std::string> m_found_source_files;

   /// Used to pull the revision numbers for the source files.
   Revision_script_manager m_revision_script_manager;

   /// A cache if last record parsed was selected or not.
   bool m_last_record_selected;

   /// Flags whether or not functions are being merged vs. added
   bool m_merging;

   /// List of function checksums to flagged for merging counts
   std::vector<unsigned> m_to_merge;

   /// Index for use with m_counts when merging count data
   unsigned m_index;

   /// The checksum of the last merged record
   unsigned m_last_merge;

   /// Cache allocation used for demangling;
   std::size_t m_demangle_size;

   /// Cache allocation used for demangling;
   char* m_demangle_buffer;
}; // End of class Parser_builder

#endif
