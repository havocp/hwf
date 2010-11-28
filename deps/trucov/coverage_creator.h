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
///  @file coverage_creator.h
///
///  @brief
///  Defines class Coverage_creator
///////////////////////////////////////////////////////////////////////////////

#ifndef COVERAGE_CREATOR_H
#define COVERAGE_CREATOR_H

// SYSTEM INCLUDES

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

// PROJECT INCLUDES

#include <boost/algorithm/string.hpp>

// LOCAL INCLUDES

#include "config.h"
#include "parser.h"
#include "tru_utility.h"

class Coverage_creator
{
public:

   // PUBLIC METHODS

   /// @brief
   /// Generates coverage files
   ///
   /// @param config  An instantiation of Config class
   /// @param parser  An instantiation of Parser class
   void generate_files( Parser &parser );

private:

   // PRIVATE METHODS

   /// @brief
   /// Generate function summary output
   ///
   /// @param rec       The current record (function)
   /// @param contents  The contents of the input source file
   /// @param source    The relative path and name of the input source file
   void do_func_summary( 
      const Record & rec, 
      const std::vector<std::string> & contents,
      const std::string & source );

   /// @brief
   /// Generate branch summary output
   ///
   /// @param rec       The current record (function)
   /// @param block     The current function block
   /// @param contents  The contents of the input source file
   /// @param source    The relative path and name of the input source file
   void do_branch_summary( const Record & rec, const Block & block,
                           const std::vector<std::string> & contents,
                           const std::string & source );

   // PRIVATE MEMBERS

   /// Output file stream
   std::ofstream outfile;
   /// Input file stream
   std::ifstream infile;
};

#endif
