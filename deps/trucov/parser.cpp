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
///  @file parser.cpp
///
///  @brief
///  Implements the methods of the Parser Class.
///
///  @remarks
///  Defines all methods of the Parser class.
///////////////////////////////////////////////////////////////////////////////

// HEADER FILE

#include "parser.h"

//  STATIC FIELD INITIALIZATION

// Null value means the object has not been instantiated yet.
Parser * Parser::ptr_instance = NULL;

// Tells the spirit parser if the current file is big endian or little endian.
bool global_little_endian = false;

unsigned int global_parsed_int32 = 0;
uint64_t global_parsed_int64 = 0;
std::string global_parsed_string = "";

//  USING STATMENTS

using std::map;
using std::vector;
using std::string;
using std::pair;
using std::exception;
using std::ifstream;
using std::cout;
using std::cerr;
using std::endl;
using std::ios;
using std::flush;

#if BOOST_VERSION < NEW_SPIRIT_VERSION
   namespace SP = boost::spirit;
#else
   namespace SP = boost::spirit::classic;
#endif

//  METHOD DEFINITIONS

bool Parser::parse_all()
{
   Tru_utility* sys_utility = Tru_utility::get_instance();
   vector< pair<string, string> > build_list;
   vector< pair<string, string> >::iterator itr;
   pair<string, string> tmp_pair;

   Config & config = Config::get_instance();
   build_list = config.get_build_files();
   itr = build_list.begin();

   string dump_file;
   int result = 0;

   cerr << "Parsing gcno and gcda files ." << flush;

   // Iterates through all pairs of gcno and gcda files and
   // parse each
   bool some_failed = false;
   for(itr = build_list.begin(); itr != build_list.end(); itr++)
   {

      // If dumping parser data
#ifdef DEBUGFLAG 
      if (config.get_flag_value(Config::Debug))
      {
         // Generate dump file
         dump_file = "selcov_dump_" +
         sys_utility->genSelcovFileName(itr->first, ".dump");
         dump_file = config.get_flag_value(Config::Output) + dump_file;

         result = parse( itr->first, itr->second, dump_file );
      }
      else // No dump file
#endif 
      {
         result = parse( itr->first, itr->second );
      }
   
      if (!result)
      {   
         cerr << "." << flush;
      }
      else
      {
         some_failed = true;
      }
   }
   cout << endl;

   if (some_failed)
   {
      cerr << "Some parsing failures occured, try:\n" 
           << " (1) Compiling source code again.\n"
           << " (2) Remove gcda files and run the executable." << endl;   
   
      return false;
   }

   // For each arc with data from the GCDA data file, assign
   // that arc's traversal count
   assign_arc_counts();

   // For each block, populate a vector with pointers to
   // the arcs entering that block
   assign_entry_arcs();

   // Calculate and assign the traversal count for all arcs
   // not assigned a count from the GCDA data file
   normalize_arcs();

   // Assign the last line of the parent block to any blocks
   // that have no line number assigned
   normalize_lines();

   // For each functions block, determine whether the block
   // is fake
   normalize_fake_blocks();

   // Populate m_blocks_sorted with Blocks sorted by line #
   // for use later by Coverage creator class
   sort_blocks();

   // Calculate total.
   calculate_total_coverage();

   return true;

} // End of Parser::parse_all(...)

////////////////////////////////////////////////////////////////////////////////
///  @brief
///  Sets up GCNO and GCDA files for parsing, calls parse function, closes files
///
///  INPUTS:
///  @param gcnoFile The string path of the gcno file
///  @param gcdaFile The string path of the gcda file
///
///  @return success(0), or failure(1)
////////////////////////////////////////////////////////////////////////////////
int Parser::parse(
    const string & gcnoFile, 
    const string & gcdaFile )
{
    mGcnoFile.open(gcnoFile.c_str());
    mGcdaFile.open(gcdaFile.c_str());

    if ( mGcnoFile.fail() )
    {
        cerr << "\nERROR: Cannot open gcno file " << gcnoFile << flush;
        mGcnoFile.close();
        mGcdaFile.close();
        return 1;
    }
    else if ( mGcdaFile.fail() )
    {
        cerr << "\nERROR: Cannot open gcda file " << gcdaFile << flush;
        mGcnoFile.close();
        mGcdaFile.close();
        return 1; 
    }

    mIsDump = false;
    Tru_utility * utility = Tru_utility::get_instance();

    m_gcno_name = utility->get_filename(gcnoFile);
    m_gcda_name = utility->get_filename(gcdaFile);
    int r = parse();

    mGcnoFile.close();
    mGcdaFile.close();

    return r;

} // end of Parser::parse(const string &, const string &)

#ifdef DEBUGFLAG 
////////////////////////////////////////////////////////////////////////////////
///  @brief
///  Sets up GCNO and GCDA files for parsing, calls parse function, closes files
///  Dumps output to dump file
///
///  INPUTS:
///  @param gcnoFile The string path of the gcno file
///  @param gcdaFile The string path of the gcda file
///  @param dumpFile The string path of the dump file
///
///  @return int success(0), or failure(1)
////////////////////////////////////////////////////////////////////////////////
int Parser::parse(
    const string & gcnoFile, 
    const string & gcdaFile,
    const string & dumpFile )
{
    // Open files
    mGcnoFile.open(gcnoFile.c_str());
    mGcdaFile.open(gcdaFile.c_str());
    mDumpFile.open(dumpFile.c_str());
    if ( mGcnoFile.fail() )
    {
        cerr << "\nERROR: Cannot open gcno file " << gcnoFile << flush;
        mGcnoFile.close();
        mGcdaFile.close();
        return 1;
    }
    else if ( mGcdaFile.fail() )
    {
        cerr << "\nERROR: Cannot open gcda file " << gcdaFile << flush;
        mGcnoFile.close();
        mGcdaFile.close();
        return 1; 
    }

    // Used with
    mIsDump = true;
    mDumpFile << "**************************************************\n"
              << "GCNO FILE: " << gcnoFile << "\n"
              << "GCDA FILE: " << gcdaFile << "\n";

    Tru_utility * utility = Tru_utility::get_instance();
    m_gcno_name = utility->get_filename(gcnoFile);
    m_gcda_name = utility->get_filename(gcdaFile);
    
    int r = parse();

    // Close files
    mGcnoFile.close();
    mGcdaFile.close();
    if (mIsDump)
    {    
        mDumpFile.close();
    }
    return r;

} // end of Parser::parse(const string &, const string &, const string &)
#endif

///////////////////////////////////////////////////////////////////////////
/// @brief
/// Parses the current gcno and gcda files pair and merges the coverage
/// data into the data structure.
///
/// @return success(true), failure(false)
///////////////////////////////////////////////////////////////////////////
int Parser::parse()
{
    try
    {
        char buf[4];
 
        // Get gcno magic
        mGcnoFile.read(buf, 4);
        unsigned int gcnoMagic = *( reinterpret_cast<unsigned int *>(buf) );

       // Get gcno file length
        mGcnoFile.seekg(0, ios::end);
        unsigned int gcnoLength = mGcnoFile.tellg();
        mGcnoFile.seekg(0, ios::beg);

        // Read in gcno file
        vector<char> gcno_buf(gcnoLength);
        mGcnoFile.read(&gcno_buf[0], gcnoLength);

        // Check for valid gcno magic
        bool gcno_little_endian = true;
        if (gcnoMagic == 0x6F6E6367)
        {
            // Set little endian
            gcno_little_endian = false;
        }
        else if (gcnoMagic != 0x67636E6F)
        {
            cerr << "\nERROR: Invalid Gcno file " << m_gcno_name << flush;
            return 1;
        }

        Config & config = Config::get_instance();
        Parser_builder parser_builder( m_source_files,
            config.get_flag_value(Config::Revision_script),
            config.get_source_files() );
       
        // Parse the gcno file
        Gcno_grammar gcnoGrammar( parser_builder, mIsDump, mDumpFile );
        global_little_endian = gcno_little_endian;
        const char * first = &gcno_buf[0];
        const char * last = &gcno_buf[0] + gcnoLength;
        SP::parse_info<> info = raw_parse(first, last, gcnoGrammar);

        if (!info.full)
        {
            cerr << "\nERROR: Failed to parse Gcno file " << m_gcno_name << flush;
            return 1;
        }

        if ( !mGcdaFile.fail() )
        {

            // Get gcda magic
            mGcdaFile.read(buf, 4);
            unsigned int gcdaMagic = *( reinterpret_cast<unsigned int *>(buf) );

            // Get gcda file length
            mGcdaFile.seekg(0, ios::end);
            unsigned int gcdaLength = mGcdaFile.tellg();
            mGcdaFile.seekg(0, ios::beg);

            // Read in gcda file
            vector<char> gcda_buf(gcdaLength);
            mGcdaFile.read(&gcda_buf[0], gcdaLength);
     
            // Check for valid gcda magic
            bool gcda_little_endian = true;
            if (gcdaMagic == 0x61646367)
            {
                // Set little endian
                gcda_little_endian = false;
            }
            else if (gcdaMagic != 0x67636461)
            {
                cerr << "\nERROR: Invalid Gcda file " << m_gcda_name << flush;
            }
            
            // Parse the gcda file
            Gcda_grammar gcdaGrammar( parser_builder, mIsDump, mDumpFile );
            global_little_endian = gcda_little_endian;
            first = &gcda_buf[0];
            last = &gcda_buf[0] + gcdaLength;
            info = raw_parse( first, last, gcdaGrammar );

            if (!info.full)
            {
                cerr << "\nERROR: Failed to parse Gcda file " << m_gcda_name << flush;
                return 1;
            }
        }
    }
    catch (exception e)
    {
        return 1;
    }

    return 0;

} // end of void Parse()

map<string, Source_file> & Parser::get_source_files()
{
    return m_source_files;
}

//////////////////////////////////////////////////////////////////////////////
///  @brief
///  Assigns each block's fromArc vector pointers to any entry arcs
///
///  @remarks
///  Pre conditions: map of records is initialized, arc list is populated.
//////////////////////////////////////////////////////////////////////////////
void Parser::assign_entry_arcs()
{
   // For all source files
   for ( map<string, Source_file>::iterator source_iter = 
            m_source_files.begin();
         source_iter != m_source_files.end();
         source_iter++ )
   { 
      map<Source_file::Source_key, Record> & records = 
      source_iter->second.m_records;
 
      // For each record (function)
      for( map<Source_file::Source_key, Record>::iterator i = records.begin(); 
           i != records.end(); 
           ++i )
      {
         unsigned source = 0;

         // For each block within function
         for( unsigned j = 0; j < i->second.m_blocks.size(); ++j )
         {
            // For each arc
            for ( unsigned k = 0; k < i->second.m_blocks[j].m_arcs.size(); ++k )
            {
               // Assign destination block a pointer to the arc entering it
               unsigned dest = i->second.m_blocks[j].m_arcs[k].m_dest_block;
               i->second.m_blocks[dest].m_from_arcs.push_back( &(i->second.m_blocks[j].m_arcs[k]) );
            }
            ++source;
         }
      }
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Assigns each counted arc the count value read from the gcda file
///
///  @return void
///
///  @remarks
///  Pre  : Map of records is initialized, gcda file has been parsed
///  Post : none
///////////////////////////////////////////////////////////////////////////
void Parser::assign_arc_counts()
{
   // For all source files
   for ( map<string, Source_file>::iterator source_iter =
            m_source_files.begin();
         source_iter != m_source_files.end();
         source_iter++ )
   {
      map<Source_file::Source_key, Record> & records = 
         source_iter->second.m_records;

      // For each record (function)
      for ( map<Source_file::Source_key, Record>::iterator i = 
               records.begin(); 
            i != records.end(); 
            ++i )
      {
         unsigned pos = 0;

         // For each block
         for ( unsigned j = 0; j < i->second.m_blocks.size(); ++j )
         {
            // For each arc
            for ( unsigned k = 0; k < i->second.m_blocks[j].m_arcs.size(); ++k )
            {
               // Test if lowest bit is not set and if so then assign arc count
               if ( ! ( i->second.m_blocks[j].m_arcs[k].m_flag & 1 ) )
               {
                  i->second.m_blocks[j].m_arcs[k].m_count =
                     static_cast<int64_t>( i->second.m_counts.at( pos ) );
                  pos++;
               }
               else
               {
                  // Arcs without GCDA data are assigned -1 before they are normalized
                  i->second.m_blocks[j].m_arcs[k].m_count = -1;
               }
            }
         }
      }
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Normalizes the arc counts between counted and non-counted arcs
///
///  @return void
///
///  @remarks
///  Pre  : Map of records is initialized, gcda file has been parsed
///  Post : All arcs have an assigned traversal count
///////////////////////////////////////////////////////////////////////////
void Parser::normalize_arcs()
{
   bool normalized;

   // For all source files
   for ( map<string, Source_file>::iterator source_iter =
            m_source_files.begin();
         source_iter != m_source_files.end();
         source_iter++ )
   {
      map<Source_file::Source_key, Record> & records = 
         source_iter->second.m_records;

      // For each record (function)
      for ( map<Source_file::Source_key, Record>::iterator i = records.begin(); 
            i != records.end(); 
            ++i )
      {
         do {
            // Initialize normalized to false
            normalized = false;

            // For each function block starting with the 2nd to last block
            for ( int j = (int) i->second.m_blocks.size() - 2; j >= 1; --j )
            {
               // If block hasn't been normalized yet and block isn't start block
               if ( i->second.m_blocks[j].m_normalized == false )
               {
                  // Set arc count, arc location, and totals to 0
                  unsigned count = 0;
                  unsigned loc   = 0;
                  unsigned arcCountTotal     = 0;
                  unsigned fromArcCountTotal = 0;

                  // For each arc in block
                  for ( unsigned k = 0; k < i->second.m_blocks[j].m_arcs.size(); ++k )
                  {
                     // If arc hasn't been normalized
                     if ( i->second.m_blocks[j].m_arcs[k].m_count == -1 )
                     {
                        // Increment count and store arc number
                        ++count;
                        loc = k;
                     }
                     else
                     {
                        // Add arc's count to arc count total
                        arcCountTotal += i->second.m_blocks[j].m_arcs[k].m_count;
                     }
                  }

                  // For each arc entering the block
                  for ( unsigned k = 0; k < i->second.m_blocks[j].m_from_arcs.size(); ++k )
                  {
                     // If arc hasn't been normalized
                     if ( i->second.m_blocks[j].m_from_arcs[k]->m_count == -1 )
                     {
                        // Increment count and store arc number
                        ++count;
                        loc = k;
                     }
                     else
                     {
                        // Add entering arc's count to entering arc count total
                        fromArcCountTotal += i->second.m_blocks[j].m_from_arcs[k]->m_count;
                     }
                  }

                  // If block has no non-normalized arcs, block is normalized
                  if ( count == 0 )
                  {
                      i->second.m_blocks[j].m_normalized = true;
                      normalized = true;
                  }
                  // If block only has one non-normalized arc, calculate its count
                  else if ( count == 1 )
                  {
                     unsigned diff = 0;

                     // Take the difference from entering and exiting arcs' counts
                     if ( arcCountTotal >= fromArcCountTotal )
                     {
                        diff = arcCountTotal - fromArcCountTotal;
                     }
                     else
                     {
                        diff = fromArcCountTotal - arcCountTotal;
                     }

                     // If number of exiting arcs is >= arc location
                     if ( i->second.m_blocks[j].m_arcs.size() >= loc + 1)
                     {
                        // Assign count to non-normalized arc
                        if ( i->second.m_blocks[j].m_arcs[loc].m_count == -1 )
                        {
                           i->second.m_blocks[j].m_arcs[loc].m_count = diff;
                        }
                        else
                        {
                           i->second.m_blocks[j].m_from_arcs[loc]->m_count = diff;
                        }
                     }
                     // Assign count to entering arc
                     else
                     {
                        i->second.m_blocks[j].m_from_arcs[loc]->m_count = diff;
                     }

                     // Mark block as normalized
                     i->second.m_blocks[j].m_normalized = true;
                     normalized = true;
                  }
               }
            }
         // Loop until no normalization has occurred
         } while ( normalized == true );
      }
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Inserts the last line # from parent for all blocks without a line #
///
///  @return void
///
///  @remarks
///  Pre  : Map of records has been initialized and populated
///  Post : All blocks have at least one assigned line #
///////////////////////////////////////////////////////////////////////////
void Parser::normalize_lines()
{
   /*
      Normalize lines does the following::
      1.) For each source file, store the first line of the function into
          a vector.
      2.) Find the last line of each function based on the above vector
      3.) Detect whether each Line struct refers to an inlined source line
      4.) For any block without any associated Line structs, inherit the
          line data from a parent block
      5.) Create a vector for each block with its non-inlined Line structs
      6.) If there are still blocks without associtates line structs,
          give them a value of -1 to flag them.
   */

   // Get instances of Config and Tru_utility classes
   Config & config = Config::get_instance();
   Tru_utility * ptr_utility = Tru_utility::get_instance();

   // For all source files
   for ( map<string, Source_file>::iterator source_iter =
            m_source_files.begin();
         source_iter != m_source_files.end();
         source_iter++ )
   {
      map<Source_file::Source_key, Record> & records = 
         source_iter->second.m_records;
      vector<unsigned> first_line;

      // Create a list of functions sorted by beginning line number
      order_by_line( first_line, records );

      // For each Record (function)
      for ( map<Source_file::Source_key, Record>::iterator i = records.begin();
            i != records.end(); ++i )
      {
         Record & rec = i->second;
         int last_line = find_last_line( first_line, rec );

         // For each block
         for ( unsigned j = 0; j < rec.m_blocks.size(); ++j )
         {
            rec.m_blocks[j].m_inlined = false;

            // For each Lines_data object
            for ( map<string, Lines_data>::iterator lines_iter = 
                  rec.m_blocks[j].m_lines.begin();
                  lines_iter != rec.m_blocks[j].m_lines.end();
                  ++lines_iter )
            {
               Lines_data & line_data = lines_iter->second;
               // Mark appropriate Line structs as inlined
               assign_inline_status( line_data, lines_iter->first, rec, last_line );
            }

            // If a block has no lines and the block is not a start or end block
            if ( rec.m_blocks[j].m_lines.empty()
                 && ! rec.m_blocks[j].is_start_block()
                 && ! rec.m_blocks[j].is_end_block() )
            {
               // Look for lines from current source file

               // If a block's parent block has line numbers and parent block
               // is not the start block
               unsigned origin_block = rec.m_blocks[j].m_from_arcs.front()->m_origin_block;
               if ( rec.m_blocks[ origin_block ].m_lines.find( rec.m_source )
                    != rec.m_blocks[ origin_block ].m_lines.end()
                    && ! rec.m_blocks[ origin_block ].is_start_block() )
               {
                  // For each Lines_data
                  for ( map<string, Lines_data>::iterator lines_iter =
                        rec.m_blocks[ origin_block ].m_lines.begin();
                        lines_iter != rec.m_blocks[ origin_block ].m_lines.end();
                        ++lines_iter )
                  {
                     Lines_data & line_data = lines_iter->second;
                     // Assign the line data of a parent block from the current
                     // source file
                     assign_line_current( line_data, rec, j );
                  }
               }
               else
               {
                  // Assign the line data of a parent block with inlined data
                  assign_line_inlined( rec.m_blocks[ origin_block ].m_lines, rec, j );
               }
            }

            // If block is not a start or end block
            if ( ! rec.m_blocks[j].is_start_block()
                 && ! rec.m_blocks[j].is_end_block() )
            {
               // For each Lines_data
               for ( map<string, Lines_data>::iterator lines_iter = 
                     i->second.m_blocks[j].m_lines.begin();
                     lines_iter != i->second.m_blocks[j].m_lines.end();
                     ++lines_iter )
               {
                  Lines_data & line_data = lines_iter->second;

                  // If Lines_data is for current source file
                  if ( rec.m_source == lines_iter->first )
                  {
                     // Create a vector of current block's non-inlined lines
                     for ( unsigned k = 0; k < line_data.m_lines.size(); ++k )
                     {
                        if ( line_data.m_lines[k].m_inlined == false )
                        {
                           rec.m_blocks[j].m_non_inlined.push_back( line_data.m_lines[k] );
                        }
                     }
                  }
               }
            }

            // If no parent blocks have associated line numbers then
            // mark block as such with a -1 line number
            if ( rec.m_blocks[j].m_lines.empty()
                 && ! rec.m_blocks[j].is_start_block()
                 && ! rec.m_blocks[j].is_end_block() )
            {
               Line l;
               l.m_line_num = -1;
               l.m_inlined = false;

               rec.m_blocks[j].m_lines[ rec.m_source ].m_lines.push_back( l );
            }
         }
      }
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Examines each record's function blocks to determine if they're fake
///
///  @return void
///
///  @pre No function blocks are marked as fake
///  @post Qualifying function blocks are flagged as fake
///////////////////////////////////////////////////////////////////////////
void Parser::normalize_fake_blocks()
{
   // For all source files
   for ( map<string, Source_file>::iterator source_iter =
            m_source_files.begin();
         source_iter != m_source_files.end();
         source_iter++ )
   {
      map<Source_file::Source_key, Record> & records = 
         source_iter->second.m_records;

      // For each Record (function)
      for ( map<Source_file::Source_key, Record>::iterator i = records.begin();
            i != records.end(); ++i )
      {
         bool done;

         // Loop until no blocks have had their fake status changed
         do
         {
            done = true;

            // For each block within the function
            for ( unsigned j = 0; j < i->second.m_blocks.size(); ++j )
            {
               Block & block_ref = i->second.m_blocks[j];
               // Only look at blocks not marked as fake and ignore the
               // function entry and exit blocks
               if ( ! block_ref.is_fake() && ! block_ref.is_start_block() )
                    //&& ! block_ref.is_end_block() )
               {
                  // First case: Check if all incoming arcs to a block are fake
                  bool found_real = false;
                  
                  for ( unsigned k = 0; k < block_ref.m_from_arcs.size(); ++k )
                  {
                     // If a non-fake entry arc is found, set found flag and
                     // exit loop
                     if ( ! block_ref.m_from_arcs[k]->is_fake() )
                     {
                        found_real = true;
                        break;
                     }
                  }

                  // Mark block as fake if no real entry arc(s) found
                  if ( ! found_real )
                  {
                     block_ref.m_fake = true;
                     done = false;
                     break;
                  }

                  // Second case: Check if all immediate "parent" blocks are
                  // fake
                  found_real = false;

                  for ( unsigned k = 0; k < block_ref.m_from_arcs.size(); ++k )
                  {
                     // If a non-fake parent block is found, set found flag
                     // and exit loop
                     if ( ! i->second.m_blocks[ block_ref.m_from_arcs[k]->m_origin_block ].is_fake() )
                     {
                        found_real = true;
                        break;
                     }
                  }

                  // Mark block as fake if a fake real parent blocks found
                  if ( ! found_real )
                  {
                     block_ref.m_fake = true;
                     done = false;
                     break;
                  }
               }
            }
         } while ( ! done );
      }
   }
}

/////////////////////////////////////////////////////////////////////////
/// Calculates the total coverage for each source and the entire project.
/////////////////////////////////////////////////////////////////////////
void Parser::calculate_total_coverage()
{
   m_coverage_percentage = 0;
   
   for ( map<string, Source_file>::iterator source_iter =
            m_source_files.begin();
         source_iter != m_source_files.end();
         source_iter++ )
   {
      const map<Source_file::Source_key, Record> & records =
         source_iter->second.m_records;

      source_iter->second.m_coverage_percentage = 0;
      for ( map<Source_file::Source_key, Record>::const_iterator rec_iter = records.begin();
            rec_iter != records.end();
            ++rec_iter )
      {
         source_iter->second.m_coverage_percentage += rec_iter->second.get_coverage_percentage();
      } 

      if ( records.size() )
      {
         source_iter->second.m_coverage_percentage /= records.size(); 
      }
      else
      {
         source_iter->second.m_coverage_percentage = 0; 
      }

      m_coverage_percentage += source_iter->second.m_coverage_percentage; 
   }

   m_coverage_percentage /= m_source_files.size();       
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Finds the starting line number of each function, stores the lines
///  into a vector and then sorts the vector
///
///  @return void
///////////////////////////////////////////////////////////////////////////
void Parser::order_by_line( vector<unsigned> & first_line,
   const map<Source_file::Source_key, Record> & records )
{
   // For each Record (function)
   for ( map<Source_file::Source_key, Record>::const_iterator i = records.begin();
         i != records.end(); ++i )
   {
      // Store the first line number of each function into a vector
      first_line.push_back( i->second.m_line_num );
   }
   std::sort( first_line.begin(), first_line.end() );
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Finds the last source line number of a record
///
///  @return int (the last line number)
///////////////////////////////////////////////////////////////////////////
int Parser::find_last_line( const std::vector<unsigned> & first_line,
      const Record & rec )
{
   int last_line = -1;

   // Find the last line of each record (function)
   for ( unsigned j = 0; j < first_line.size(); ++j )
   {
      if ( rec.m_line_num == first_line[j] )
      {
         // Unless function is the last function in a source file,
         // assign record's last line to equal the next function's
         // first line - 1
         if ( j != first_line.size() - 1 )
         {
            last_line = first_line[j+1] - 1;
         }
      }
   }

   return last_line;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Marks a Line struct as inlined if it meets certain criteria
///
///  @return void
///////////////////////////////////////////////////////////////////////////
void Parser::assign_inline_status( Lines_data & line_data, const string & source,
   Record & rec, const unsigned last_line )
{
   // Get instances of Config and Tru_utility classes
   Config & config = Config::get_instance();
   Tru_utility * ptr_utility = Tru_utility::get_instance();

   string source_path;
   std::sort( line_data.m_lines.begin(), line_data.m_lines.end(), compare_lines);

   // If line data comes from a source file not within project,
   // mark lines as inlined
   if ( ! ptr_utility->is_within_project( config.get_source_files(), source, source_path ) )
   {
      for ( unsigned k = 0; k < line_data.m_lines.size(); ++k )
      {
         line_data.m_lines[k].m_inlined = true;
      }
   }
   // If line data comes from a source file other than the current
   // source file, mark lines as inlined
   else if ( source != rec.m_source )
   {
      for ( unsigned k = 0; k < line_data.m_lines.size(); ++k )
      {
         line_data.m_lines[k].m_inlined = true;
      }
   }
   else
   {
      // For each line within the current record and current source file
      for ( unsigned k = 0; k < line_data.m_lines.size(); ++k )
      {
         // Mark line as inlined if line number is less than the first
         // line number of the function
         if ( line_data.m_lines[k].m_line_num < rec.m_line_num )
         {
            line_data.m_lines[k].m_inlined = true;
         }
         // Mark line as inlined if line number is greater than last line
         // number of the function, except if function is the last function
         // of the source file
         else if ( last_line != -1
                   && line_data.m_lines[k].m_line_num > last_line )
         {
            line_data.m_lines[k].m_inlined = true;
         }
      }
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Assigns a block with no lines a line from one of its parent blocks
///  from the current source file
///
///  @return void
///////////////////////////////////////////////////////////////////////////
void Parser::assign_line_current( Lines_data & line_data, Record & rec,
   unsigned block_no )
{
   Line line;

   // For each source line
   for ( unsigned k = 0; k < line_data.m_lines.size(); ++k )
   {
      // If source line is not inlined, store Line into current block
      if ( line_data.m_lines[k].m_inlined == false )
      {
         line = line_data.m_lines[k];
      }
   }

   rec.m_blocks[block_no].m_lines[ rec.m_source ].m_lines.push_back( line );

   // If no non-inlined source lines, store first inlined line
   // into current block
   if ( rec.m_blocks[block_no].m_lines.empty() )
   {
      rec.m_blocks[block_no].m_lines[ rec.m_source ].m_lines.push_back( line_data.m_lines.front() );
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Assigns a block with no lines a line from one of its parent blocks
///  from an outside source file
///
///  @return void
///////////////////////////////////////////////////////////////////////////
void Parser::assign_line_inlined( map<string, Lines_data> & lines, Record & rec, unsigned block_no )
{
   // Get instances of Config and Tru_utility classes
   Config & config = Config::get_instance();
   Tru_utility * ptr_utility = Tru_utility::get_instance();

   // Look for lines from a project source file

   // For each Lines_data
   for ( map<string, Lines_data>::iterator lines_iter = lines.begin();
         lines_iter != lines.end(); ++lines_iter )
   {
      Lines_data & line_data = lines_iter->second;

      // If source line is from an inlined file within the project
      // store first line into current block
      string source_path;
      if ( ! ptr_utility->is_within_project( config.get_source_files(), lines_iter->first, source_path ) )
      {
         rec.m_blocks[block_no].m_lines[ lines_iter->first ].m_lines.push_back( line_data.m_lines.front() );
         break;
      }
   }

   // Else, look for lines from an outside source file
   if ( rec.m_blocks[block_no].m_lines.empty() )
   {
      // For each Lines_data
      for ( map<string, Lines_data>::iterator lines_iter = lines.begin();
            lines_iter != lines.end(); ++lines_iter )
      {
         Lines_data & line_data = lines_iter->second;

         // Store source line from outside source file into current block
         rec.m_blocks[block_no].m_lines[ lines_iter->first ].m_lines.push_back( line_data.m_lines.front() );
         break;
      }                  
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Populate m_blocks_sorted with Blocks sorted by line #
///
///  @return void
///
///  @pre m_blocks_sorted is empty
///  @post m_blocks_sorted contains a vector of Blocks sorted by line #
///////////////////////////////////////////////////////////////////////////
void Parser::sort_blocks()
{
   // For all source files
   for ( map<string, Source_file>::iterator source_iter =
            m_source_files.begin();
         source_iter != m_source_files.end();
         source_iter++ )
   {
      map<Source_file::Source_key, Record> & records = 
         source_iter->second.m_records;

      // For each Record (function)
      for ( map<Source_file::Source_key, Record>::iterator i = records.begin();
            i != records.end(); ++i )
      {
         i->second.m_blocks_sorted = i->second.m_blocks;
         std::sort( i->second.m_blocks_sorted.begin() + 1, 
                    i->second.m_blocks_sorted.end() - 1, compare_line_nums);
      }
   }
}

/////////////////////////////////////////////////////////////////////////
///
///  @brief
///  Returns true if first Block's last line # is < right Block's
///
///  @return bool
///////////////////////////////////////////////////////////////////////////
const bool compare_line_nums( const Block & lhs, const Block & rhs )
{
   const vector<Line> & lines_lhs = lhs.get_non_inlined();
   const vector<Line> & lines_rhs = rhs.get_non_inlined();

   if ( ! lines_lhs.empty() && ! lines_rhs.empty() )
   {
      unsigned line_lhs = lines_lhs.back().m_line_num;
      unsigned line_rhs = lines_rhs.back().m_line_num;

      return line_lhs < line_rhs;
   }
   else
   {
      return ! lines_lhs.empty();
   }
}

/////////////////////////////////////////////////////////////////////////
///
///  @brief
///  Returns true if left Line number < right Line number
///
///  @return bool
///////////////////////////////////////////////////////////////////////////
const bool compare_lines( const Line & lhs, const Line & rhs )
{
   return lhs.m_line_num < rhs.m_line_num;
}
