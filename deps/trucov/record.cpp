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
///  @file record.cpp
///
///  @brief
///  Implements the methods of the Record Class
///
///  @remarks
///  Defines all methods of the Record class
///////////////////////////////////////////////////////////////////////////////

// HEADER FILE

#include "record.h"

using std::vector;
using std::string;
using std::map;
using std::ifstream;
using std::ofstream;
using std::ios;
using std::cout;
using std::endl;

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns the associated source file's line numbers and inline status
///  for the lines that make up a block
///
///  @return const vector<Line> &
///////////////////////////////////////////////////////////////////////////
const vector<Line> & Lines_data::get_lines() const
{
   return m_lines;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not an Arc object is fake depending on its flags
///
///  @return true if the Arc is fake
///////////////////////////////////////////////////////////////////////////
const bool Arc::is_fake() const
{
   return ( m_flag == 2 || m_flag == 3 );
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not an arc has been taken
///
///  @return true if the Arc has been traversed
///
///  @pre The function's Arc counts have been normalized
///////////////////////////////////////////////////////////////////////////
const bool Arc::is_taken() const
{
   // Return true if the arc's count is greater than 0
   return ( m_count > 0 );
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns an Arc object's count
///
///  @return The Arc's traversal count
///////////////////////////////////////////////////////////////////////////
const int64_t Arc::get_count() const
{
   return m_count;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns an Arc object's destination block number
///
///  @return The Arc's destination block number
///////////////////////////////////////////////////////////////////////////
const unsigned Arc::get_dest() const
{
   return m_dest_block;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns an Arc object's flag value
///
///  @return The Arc's flag value
///////////////////////////////////////////////////////////////////////////
const unsigned Arc::get_flag() const
{
   return m_flag;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not a function block is an end block
///
///  @return True if the Block is an end block
///////////////////////////////////////////////////////////////////////////
const bool Block::is_end_block() const
{
   return ( m_arcs.empty() );
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not a function block is a function's start block
///
///  @return True if the Block is a starting block
///////////////////////////////////////////////////////////////////////////
const bool Block::is_start_block() const
{
   return ( m_from_arcs.empty() );
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not a function block is a branch
///
///  @return True if the Block is a branch
///////////////////////////////////////////////////////////////////////////
const bool Block::is_branch() const
{
   unsigned count = 0;

   // For each outgoing arc in the block
   for ( unsigned i = 0; i < m_arcs.size(); ++i )
   {
      // Increment the count if the arc is not fake
      if ( ! m_arcs[i].is_fake() )
      {
         ++count;
      }
   }

   // Return true if there is more than one outgoing arc
   return ( count > 1 );
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not a function block is a fake block
///
///  @return True if the Block is a fake block
///////////////////////////////////////////////////////////////////////////
const bool Block::is_fake() const
{
   return m_fake;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not a function block is made up of inlined lines
///
///  @return True if the Block is inlined
///////////////////////////////////////////////////////////////////////////
const bool Block::is_inlined() const
{
   return m_inlined;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not a function block has been normalized
///
///  @return True if the block has been normalized
///////////////////////////////////////////////////////////////////////////
const bool Block::is_normalized() const
{
   return m_normalized;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not a function block has complete coverage
///
///  @return True if Block has full coverage
///
///  @pre The record's Arc counts have been normalized
///////////////////////////////////////////////////////////////////////////
const bool Block::has_full_coverage() const
{
   // If block has outgoing arcs
   if ( ! is_end_block() )
   {
      // If the block is fake
      if ( is_fake() )
      {
         // Return false if any outgoing arc has not been taken
         for ( unsigned i = 0; i < m_arcs.size(); ++i )
         {
            if ( ! m_arcs[i].is_taken() )
            {
               return false;
            }
         }

         return true;
      }
      else
      {
         // If block is not fake, then check 2 cases:

         // Check if block has any normal outgoing arcs
         bool m_normal = false;
         for ( unsigned i = 0; i < m_arcs.size(); ++i )
         {
            if ( ! m_arcs[i].is_fake() )
            {
               m_normal = true;
               break;
            }
         }

         // If block only has fake outgoing arcs
         if ( ! m_normal )
         {
            // Case 1: normal block only has fake outgoing arcs
            // (i.e. a throw)
            // Return false if any outgoing arc has not been taken
            for ( unsigned i = 0; i < m_arcs.size(); ++i )
            {
               if ( ! m_arcs[i].is_taken() )
               {
                  return false;
               }
            }

            return true;
         }
         else
         {
            // Case 2: Normal block has at least one normal outgoing arc
            // Return false if any incoming non-fake arc has not been taken
            for ( unsigned i = 0; i < m_arcs.size(); ++i )
            {
               if ( ! m_arcs[i].is_taken() && ! m_arcs[i].is_fake() )
               {
                  return false;
               }
            }

            return true;
         }
      }
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns whether or not a function block has partial coverage
///
///  @return True if Block has partial coverage
///
///  @pre The record's Arc counts have been normalized
///////////////////////////////////////////////////////////////////////////
const bool Block::has_partial_coverage() const
{
   if ( ! is_end_block() )
   {
      // If block is fake
      if ( is_fake() )
      {
         // Return true if any arc has been taken
         {
            for ( unsigned i = 0; i < m_arcs.size(); ++i )
            {
               if ( m_arcs[i].is_taken() )
               {
                  return true;
               }
            }

            return false;
         }
      }
      // Block is not fake
      else
      {
         // Return true if any non-fake arc has been taken
         for ( unsigned i = 0; i < m_arcs.size(); ++i )
         {
            if ( m_arcs[i].is_taken() && ! m_arcs[i].is_fake() )
            {
               return true;
            }
         }

         return false;
      }
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns the block number of a function block
///
///  @return A function block's assigned number
///////////////////////////////////////////////////////////////////////////
const unsigned Block::get_block_no() const
{
   return m_block_no;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns a const references a function's vector of Arc objects
///
///  @return A reference to all of a function's outgoing Arcs
///
///  @pre A function block has outgoing arcs
///////////////////////////////////////////////////////////////////////////
const vector<Arc> & Block::get_arcs() const
{
   return m_arcs;
}


/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns a map of the line data from each source file that makes up
///  the block
///
///  @return const unsigned
///////////////////////////////////////////////////////////////////////////
const std::map<std::string,Lines_data> & Block::get_lines() const
{
   return m_lines;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns a branches total amount of taken arcs
///
///  @return const unsigned
///////////////////////////////////////////////////////////////////////////
const unsigned Block::get_branch_arc_taken() const
{
   unsigned total = 0;

   if ( is_branch() )
   {
      // For every arc in the branch
      for ( unsigned i = 0; i < m_arcs.size(); ++i )
      {
         // If the branch isn't fake and has a count, increment total
         if ( ! m_arcs[i].is_fake() && m_arcs[i].get_count() > 0 )
         {
            ++total;
         }
      }
   }

   return total;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns a branches total amount of arcs
///
///  @return const unsigned
///////////////////////////////////////////////////////////////////////////
const unsigned Block::get_branch_arc_total() const
{
   unsigned total = 0;

   if ( is_branch() )
   {
      // For every arc in the branch
      for ( unsigned i = 0; i < m_arcs.size(); ++i )
      {
         // If the branch isn't fake, increment total
         if ( ! m_arcs[i].is_fake() )
         {
            ++total;
         }
      }
   }

   return total;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns the total number of times a block has been executed
///
///  @return const unsigned
///////////////////////////////////////////////////////////////////////////
const int64_t Block::get_count() const
{
   int64_t count = 0;

   // Don't get count information on entry and exit blocks
   if ( ! is_start_block() && ! is_end_block() )
   {
      // For every incoming arc
      for ( unsigned i = 0; i < m_from_arcs.size(); ++i )
      {
         // Increment total count by incoming arc count
         count += m_from_arcs[i]->get_count();
      }
   }

   return count;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns the block's associated source file's non-inlined line numbers
///
///  @return const vector<Line> &
///////////////////////////////////////////////////////////////////////////
const vector<Line> & Block::get_non_inlined() const
{
   return m_non_inlined;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns an HTML friendly format of a function's demangled name
///
///  @return const string
///////////////////////////////////////////////////////////////////////////
const string Record::get_HTML_name() const
{
   string temp_string = "";

   // For each character in the string
   for ( unsigned i = 0; i < m_name_demangled.size(); ++i )
   {
      const char c = m_name_demangled[i];

      switch( c )
      {
         case '<':
            // Replace '<' with "&lt;"
            temp_string.append("&lt;");
            break;
         case '>':
            // Replace '>' with "&gt;"
            temp_string.append("&gt;");
            break;
         case '"':
            // Replace '"' with "&quot;"
            temp_string.append("&quot;");
            break;
         case '&':
            // Replace '&' with "&amp;"
            temp_string.append("&amp;");
            break;
         default:
            // If not a special character, push back
            temp_string.push_back( c );
      }
   }

   return temp_string;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns the total number of taken arcs within the branches of
///  a function
///
///  @return const unsigned
///
///  @pre Arc counts have been normalized
///////////////////////////////////////////////////////////////////////////
const unsigned Record::get_function_arc_taken() const
{
   unsigned total = 0;

   // For each block in function
   for ( unsigned i = 0; i < m_blocks.size(); ++i )
   {
      // If the block is a branch
      if ( m_blocks[i].is_branch() && ! m_blocks[i].is_fake() )
      {
         // Get the block's arcs
         const vector<Arc> & arcs = m_blocks[i].get_arcs();

         // For each of the block's arcs
         for ( unsigned j = 0; j < arcs.size(); ++j )
			{
            // If the arc isn't fake
            if ( ! arcs[j].is_fake() )
            {
               // Increment the total if the arc's been taken
               if ( arcs[j].is_taken() )
               {
                  ++total;
               }
            }
         }
      }
   }

   return total;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns the total number of arcs within the branches of a function
///
///  @return const unsigned
///////////////////////////////////////////////////////////////////////////
const unsigned Record::get_function_arc_total() const
{
   unsigned total = 0;

   // For each block in function
   for ( unsigned i = 0; i < m_blocks.size(); ++i )
   {
      // If the block is a branch
      if ( m_blocks[i].is_branch() && ! m_blocks[i].is_fake() )
      {
         // Get the block's arcs
         const vector<Arc> & arcs = m_blocks[i].get_arcs();

         // For each of the block's arcs
         for ( unsigned j = 0; j < arcs.size(); ++j )
         {
            // Increment the total if the arc isn't fake
            if ( ! arcs[j].is_fake() )
            {
               ++total;
            }
         }
      }
   }

   return total;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns the total number of times a function has been executed
///
///  @return const uint64_t
///////////////////////////////////////////////////////////////////////////
const uint64_t Record::get_execution_count() const
{
   uint64_t total = 0;

   // Get the first block's arcs
   const vector<Arc> & arcs = m_blocks[0].get_arcs();

   for ( unsigned i = 0; i < arcs.size(); ++i )
   {
      total += arcs[i].get_count();
   }

   return total;
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns the branch coverage percentage of a function
///
///  @return const double
///////////////////////////////////////////////////////////////////////////
const double Record::get_coverage_percentage() const
{
   double num = get_function_arc_taken();
   double den = get_function_arc_total();

   // If function has no branch arcs
   if ( den == 0 )
   {
      // Get the outgoing arcs of the entry block
      uint64_t count = get_execution_count();

      if ( count > 0 )
      {
         return 1.0;
      }
      // Otherwise, function has no coverage
      else
      {
         return 0.0;
      }
   }
   else
   {
      return num / den;
   }
}

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns true if the lhs record's starting line number is less than
///  the rhs record's starting line number
///
///  @return bool
///////////////////////////////////////////////////////////////////////////
bool record_line_lessthan(const Record * const lhs, const Record * const rhs)
{
   return lhs->m_line_num < rhs->m_line_num;
} 

/////////////////////////////////////////////////////////////////////////
///  @brief
///  Returns true if the lhs record name is less than the rhs record name
///
///  @return bool
///////////////////////////////////////////////////////////////////////////
bool record_name_lessthan(const Record * const lhs, const Record * const rhs)
{
   return lhs->m_name_demangled < rhs->m_name_demangled;
}
