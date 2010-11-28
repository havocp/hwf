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
///  @file parser_builder.cpp
///
///  @brief
///  Implements the Parser_builder class methods.
///////////////////////////////////////////////////////////////////////////////

// LOCAL INCLUDES

#include <cxxabi.h>

#include "parser_builder.h"

// USING STATEMENTS

using std::string;
using std::map;
using std::vector;

// FUNCTION DEFINITIONS

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Initializes the Parser_builder and stores a reference to the internal
/// data of the Parser class.
///////////////////////////////////////////////////////////////////////////////
Parser_builder::Parser_builder( 
   map<string, Source_file> & source_files,
   const string & revision_script_path,
   vector<string> found_source_files )
   : m_source_files( source_files ),
     m_revision_script_manager( revision_script_path ),
     m_demangle_size(64),
     m_demangle_buffer(reinterpret_cast<char*>(std::malloc(m_demangle_size)))
{
   m_found_source_files = found_source_files;
} // End of Parser_builder constructor

///////////////////////////////////////////////////////////////////////////////
Parser_builder::~Parser_builder( )
{
	std::free( m_demangle_buffer );
}

//////////////////////////////////////////////////////////////////////////////
/// @brief 
/// Will add or merge a record into the m_records data in the correct
/// source file.
/// 
/// @param rIdent The record unique indentifier.
/// @param rChecksum The record unique checksum.
/// @param rSource The record source file name.
/// @param rName The record source name.
/// @param m_name_demangled The demangled name of the record source name. 
//////////////////////////////////////////////////////////////////////////////
void Parser_builder::store_record( 
   unsigned int rIdent, 
   unsigned int rChecksum,
   const string & rSource, 
   const string & rName,
   unsigned int rLineno )
{
   // New record, so set merging mode to false
   m_merging = false;

   int status;
   char* buffer = abi::__cxa_demangle( rName.c_str(), m_demangle_buffer, &m_demangle_size, &status );
   string rec_name;
   if( buffer )
   {
      // Demangling was successful.
      assert( status == 0 );

      m_demangle_buffer = buffer;
      rec_name.assign(buffer);
   }
   else
   {
      // Demangling should never fail other than when the name wasn't demangleable.
      assert( status == -2 );

      rec_name = rName;
   }
   assert(m_demangle_size != 0 );

   // Get instances of Selector and Tru_utility classes
   Selector & selector = Selector::get_instance();
   Tru_utility * ptr_utility = Tru_utility::get_instance();
   
   string source_path;

   // If source file and fuction are both within project and selected
   if ( ptr_utility->is_within_project( m_found_source_files, rSource, 
      source_path ) && selector.is_selected( rSource, rec_name ) )
   {
      // Get source file 
      m_source_files[rSource].m_source_path = source_path;
 
      // Get revision number if revision script is supplied
      string revision_number = m_revision_script_manager.get_revision_number(
         ptr_utility->get_abs_path( source_path ) );  
      if ( revision_number.size() != 0 )
      {
         m_source_files[rSource].m_revision_number = revision_number; 
      }

      // Remember this record was selected.
      m_last_record_selected = true;      
      map<Source_file::Source_key, Record> & records_data = 
        m_source_files[rSource].m_records; 

      // Assign record ident
      records_data[rChecksum].m_ident    = rIdent;
      // Mark current record as the last record stored
      set_last_record( records_data, rChecksum );

      // Check record's block size to determine if record already has data
      if ( ! get_last_record().m_blocks.empty() )
      {
         // If record already has data, trigger merge mode
         m_merging = true;
         bool found = false;
         // Check if record is flagged for merging yet
         for ( unsigned i = 0; i < m_to_merge.size(); ++i )
         {
            if ( m_to_merge[i] == rChecksum )
            {
               found = true;
               break;
            }
         }
         // If not flagged yet, add to list of records to be merged
         if ( ! found )
         {
            m_to_merge.push_back( rChecksum );
         }
      }
      // New record, so fill data members
      else
      {
         get_last_record().m_checksum = rChecksum;
         get_last_record().m_name     = rName;
         get_last_record().m_source   = rSource;
         get_last_record().m_name_demangled = rec_name;
         get_last_record().m_line_num = rLineno;
      }
   }
   else // Remember this record was not selected.
   {
      m_last_record_selected = false;
   }
} // End of Parser_builder::add_record(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Will add or merge the b_flags into the last record touched.
/// 
/// @param bLength The length of the block record.
/// @param bFlags The block flags.
/// @param bIteration The current iteration of this block
//////////////////////////////////////////////////////////////////////////////
void Parser_builder::store_blocks(
   unsigned int bLength, 
   unsigned int bFlags, 
   unsigned int bIteration )
{
   // Only add record's blocks if not in merging mode
   if ( ! m_merging )
   {
      if ( m_last_record_selected )
      {
         // Construct block and add it to current record
         Block b( bIteration );
         get_last_record().m_blocks.push_back( b );
      }
   }
} // End of Parser_builder::store_blocks(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Stores outgoing arc data to the last record added to a specified block.
///
/// @param aBlockno The source blocks unique indentifier for the last record.
/// @param aDestBlock The destination blocks unique indentifier for the last
///                   record.
/// @param aFlags The arc flags.
//////////////////////////////////////////////////////////////////////////////
void Parser_builder::store_arcs(
   unsigned int aBlockno,
   unsigned int aDestBlock,
   unsigned int aFlags )
{
   // Only add block's arcs if not in merging mode
   if ( ! m_merging )
   {
      if ( m_last_record_selected )
      {
         // Construct arc and add it to current block
         Arc arc( aDestBlock, aBlockno, aFlags );
         get_last_record().m_blocks[aBlockno].m_arcs.push_back( arc );
      }
   }
} // End of Parser_builder::store_arcs(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Stores the lineno associated with a block
///
/// @param lBlockno The indentifier of the block containing the lineno
/// @param lLineno The line number.
//////////////////////////////////////////////////////////////////////////////
void Parser_builder::store_line_number(
   unsigned int lBlockno,
   unsigned int lLineno,
   const string & lName )
{
   // Only add lines data to block if not in merging mode
   if ( ! m_merging )
   {
      if ( m_last_record_selected )
      {
         // Add source file line into associated Lines_data entry
         bool found = false;
         vector<Line> & lines = get_last_record().m_blocks[lBlockno].m_lines[lName].m_lines;
         unsigned count = lines.size();

         // Check if line number already exists
         for ( unsigned i = 0; i < count; ++i )
         {
            if ( lines[i].m_line_num == lLineno )
            {
               found = true;
            }
         }

         // If line number is new, push onto vector
         if ( ! found )
         {
            Line l;
            l.m_line_num = lLineno;
            l.m_inlined = false;

            lines.push_back( l );
         }
      }
   }
} // End of Parser_builder::store_line_number(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Stores the arc count in the last record added.
///
/// @param aCount The arc count.
//////////////////////////////////////////////////////////////////////////////
void Parser_builder::store_count( 
   unsigned int rIdent,
   unsigned int rChecksum,
   uint64_t aCount )
{
   // Search through all source objects
   for ( map<string, Source_file>::iterator it = m_source_files.begin();
         it != m_source_files.end();
         it++ )
   {
      map<Source_file::Source_key, Record> & records = 
         it->second.m_records;   
      
      // If record in this source file, then store it
      if ( records.find( rChecksum ) != records.end() )
      {
         bool found = false;
         // Check if function is flagged for count merging
         for ( unsigned i = 0; i < m_to_merge.size(); ++i )
         {
            if ( m_to_merge[i] == rChecksum )
            {
               found = true;
               break;
            }
         }
         // If not flagged to be merged, push back count data
         if ( ! found )
         {
            records[rChecksum].m_counts.push_back( aCount );
            break;
         }
         // If flagged to be merged
         else
         {
            // If record has changed, reset index last checksum merged
            if ( rChecksum != m_last_merge )
            {
               m_last_merge = rChecksum;
               m_index = 0;
            }
            // Add new count information to total and increment index
            records[rChecksum].m_counts[m_index] += aCount;
            ++m_index;
         }
      }
   }   
} // End of Parser_builder::store_count
