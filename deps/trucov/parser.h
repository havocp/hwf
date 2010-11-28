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
///  @file parser.h
///
///  @brief
///  Prototype GCNO/GCDA Parser
///
///  @remarks
///  Defines class Parser
///
///  Requirements Specification: < Null >
///
///  Design Description: < Null >
///
///////////////////////////////////////////////////////////////////////////////
#ifndef PARSER_H
#define PARSER_H

// SYSTEM INCLUDES

#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <exception>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>
#include <map>

// PROJECT INCLUDES

#include <boost/version.hpp>

#if BOOST_VERSION < NEW_SPIRIT_VERSION
   #include <boost/spirit/core.hpp>
   #include <boost/spirit/tree/ast.hpp>
   #include <boost/spirit/tree/parse_tree.hpp>
#else
   #include <boost/spirit/include/classic_core.hpp>
   #include <boost/spirit/include/classic_ast.hpp>
   #include <boost/spirit/include/classic_parse_tree.hpp>
#endif

// LOCAL INCLUDES

#include "parser_builder.h"
#include "gcno_grammar.h"
#include "gcda_grammar.h"
#include "record.h"
#include "config.h"

// DEFINTIIONS

///////////////////////////////////////////////////////////////////////////////
///  @class Parser
///
///  @brief
///  Parser gcno and gcda files
///
///  @remarks
///  Class takes a name of a string when constructed, and then parses that
///  file when the parse method is called
///////////////////////////////////////////////////////////////////////////////
class Parser
{
public:

   // PUBLIC METHODS

	/// Destructor
	///
   /// @brief
   /// Clean up the instance memory
   ~Parser()
   {
      if (ptr_instance != NULL)
      {
         delete ptr_instance;
      }
   }

   /// @brief
   /// Returns the single instance of this singleton.
   ///
   /// @return Returns a reference to the singleton Parser object
   static Parser & get_instance()
   {
      // Create instance is non existant
      if ( Parser::ptr_instance == NULL )
      {
         ptr_instance = new Parser();
      }

      return *ptr_instance;
   } // end of get_instance()

   ///////////////////////////////////////////////////////////////////////////
   /// Constructor with input file
   ///
   /// @param a an integer argument
   ///
   /// @remarks
   /// Constructs the Parser object and opens the input file given
   ///////////////////////////////////////////////////////////////////////////
   Parser(const char * gcnoFile, const char * gcdaFile);

   ///////////////////////////////////////////////////////////////////////////
   ///
   ///  @return success(0), failure(-1)
   ///
   ///  @remarks
   ///  Parses the entire input file
   ///  Pre  : infile points to a valid input file stream
   ///  Post : Parse Tree is constructed and all data on the program and test
   ///         coverage has been gathered/calculated.
   ///  Throw: IOException
   ////////////////////////////////////////////////////////////////////////////
   int parse();

   int parse(
       const std::string & gcnoFile, 
       const std::string & gcdaFile,
       const std::string & dumpFile );

   int parse(
       const std::string & gcnoFile, 
       const std::string & gcdaFile );

   bool parse_all(); 

   double get_coverage_percentage() const
   {
      return m_coverage_percentage;
   }

   std::map<std::string, Source_file> & get_source_files();

   std::map<unsigned int, Record> & get_records();

   // PUBLIC VARAIBLES

	static const int TagLength = 4;


#ifdef DEBUGFLAG

   bool gui_dump( std::ofstream & out )
   {
      std::map<std::string, Source_file> & sources = get_source_files(); 

      for ( std::map<std::string, Source_file>::iterator it_src = sources.begin();
         it_src != sources.end();
         ++it_src )
      {
         out << it_src->second.m_source_path << "\n" 
             << it_src->second.m_coverage_percentage * 100 << "%\n";


         std::map<unsigned int, Record> & records = it_src->second.m_records;
         for ( std::map<unsigned int, Record>::iterator it_rec = records.begin();
            it_rec != records.end();
            ++it_rec )
         {
            out  << it_rec->second.m_name_demangled << "\n" 
                 << it_rec->second.m_report_path << "\n"
                 << it_rec->second.m_graph_path << "\n"
                 << it_rec->second.m_line_num << "\n";

            double den = it_rec->second.get_function_arc_total();
            if ( den == 0 )
            {
               double percentage = it_rec->second.get_coverage_percentage();
               out << percentage * 100 << "%\n";
            }
            else
            {
               double num = it_rec->second.get_function_arc_taken();
               double percentage = it_rec->second.get_coverage_percentage();
               out << percentage * 100 << "%\n";
            }
         } 
         out << "@@@__END_OF_SOURCE__@@@" << "\n";
      }
      out << "@@@__END_OF_EVERYTHING__@@@" << "\n";
   }   

#endif

private:

   // PRIVATE METHODS

   /// @brief
   /// Constructs the Parser object, and initializes private data fields.
   ///
   /// @remarks
   /// Private default constructor required for singleton.
   explicit Parser()
   {
      // void
   }

   /// @brief
   /// Constructs this object to be exactly the same as the source.
   ///
   /// @remarks
   /// Private copy constructor required for singleton.
   explicit Parser( const Parser & source )
   {
      // void
   }

   /// @brief
   /// Assigns this objects state to be the same as the source object.
   ///
   /// @remarks
   /// Private copy constructor required for singleton.
   Parser & operator=( const Parser & source )
   {
      return *this;
   }

   /// Populate each Block's m_from_arcs with incoming Arc pointers
   void assign_entry_arcs();
   /// Assign each counted Arc a value
   void assign_arc_counts();
   /// Calculate the traversal count of each uncounted Arc
   void normalize_arcs();
   /// Populate the Lines data for all blocks without line information
   void normalize_lines();
   /// Calculate whether each block is fake
   void normalize_fake_blocks();
   /// Populate m_blocks_sorted with Blocks sorted by line #
   void sort_blocks();
   /// Calculates the total coverage for each source and the entire project.
   void calculate_total_coverage(); 
   /// Create a vector with the first source line of each function within
   /// a source file
   void order_by_line( std::vector<unsigned> & first_line,
      const std::map<Source_file::Source_key, Record> & records );
   /// Find the last source line of a record
   int find_last_line( const std::vector<unsigned> & first_line,
      const Record & rec );
   /// Mark a line number as inlined if it meets certain criteria
   void assign_inline_status( Lines_data & line_data, const std::string & source,
      Record & rec, const unsigned last_line );
   /// Get a line from the current source file
   void assign_line_current( Lines_data & line_data, Record & rec,
      unsigned block_no );
   /// Get a line from an outside source file
   void assign_line_inlined( std::map<std::string, Lines_data> & lines, Record & rec, unsigned block_no );

   // PRIVATE VARAIBLES

   std::ifstream    mGcnoFile;                 // Gcno file stream
   std::ifstream    mGcdaFile;                 // Gcda file stream
   std::ofstream    mDumpFile;
   bool        mIsDump;
   std::string      mMagic;                    // Magic
   std::string      mVersion;                  // Version of the input file
   std::string      mStamp;                    // Stamp of the file

   std::string      m_gcno_name;
   std::string      m_gcda_name;

   bool        mIsGCNO;
   bool        mIsLittleEndian;

   double m_coverage_percentage;

   /// Pointer to the singleton instance of Dot_creator.
   static Parser * ptr_instance;
   /// Map of all source files within the project
   std::map<std::string, Source_file> m_source_files;

}; // end of class Parse

/// Compares the last line numbers of two blocks to determine order
const bool compare_line_nums( const Block & lhs, const Block & rhs );

/// Compares two Line structs to determine order
const bool compare_lines( const Line & lhs, const Line & rhs );

#endif
