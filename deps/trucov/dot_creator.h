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
///  @file dot_creator.h
///
///  @brief
///  Defines class Dot_creator
///////////////////////////////////////////////////////////////////////////////

#ifndef DOT_CREATOR_H
#define DOT_CREATOR_H

// SYSTEM INCLUDES

#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <boost/filesystem/operations.hpp>
#include <stdint.h>
#include <cctype>

// LOCAL INCLUDES

#include "config.h"
#include "parser.h"

/// @brief
/// Generates DOT files from GCNO and GCDA data
class Dot_creator
{
public:

   /// @brief
   /// Type of render.
   enum Render_type 
   {
      None,
      Pdf,
      Svg
   };

   /// PUBLIC METHODS

   /// @brief
   /// Initializes the dot attributes.
   Dot_creator();

   /// @brief
   /// Generates one complete coverage file
   ///
   /// @param parser  An instantiation of Parser class
   void generate_file( Parser & parser );

   /// @brief
   /// Generates coverage files
   ///
   /// @param parser  An instantiation of Parser class
   void generate_files( Parser & parser );

   /// @brief
   /// Creates a render file if a render type is set.
   ///
   /// @param output_dir The destination output directory path.
   /// @param source_path The destination file name.
   /// @param full_path The full path to the destination file
   void create_render_file(
      const std::string & dot_file,
      const std::string & output_file,
      const bool append_extension);

   /// @brief
   /// Sets the render type of the next run.
   ///
   /// @param render_type Type of render of next run.  
   void set_render_type( std::string render_type );

private:

   /// PRIVATE METHODS

   /// Generates the arc information in the DOT file
   void generate_arcs( const Record & rec );
   /// Generates the block information in the DOT file
   void generate_blocks( const Record & rec );
   /// Generate the record's header block
   void create_header( const Record & rec );
   /// Determine block shape
   void output_shape( const Block & block );
   /// Determine line style and color
   void output_line_style( const Block & block );

   /// PRIVATE MEMBERS

   /// Output file stream
   std::ofstream outfile;

   /// List of source files within project
   std::vector<std::string> src_files;  

   /// Current render type.
   Render_type m_render_type;

   /// The normal block shape.
   std::string m_normal_block_shape;

   /// The normal block style.
   std::string m_normal_block_style;

   /// The fake block shape.
   std::string m_fake_block_shape;

   /// The fake block shape.
   std::string m_fake_block_style;

   /// The branch block shape.
   std::string m_branch_block_shape;

   /// The start block shape.
   std::string m_function_block_shape;

   /// The end block shape.
   std::string m_end_block_shape;

   /// The style used for fake arcs and blocks.
   std::string m_fake_style;
   
   /// The style used for normal arcs and blocks.
   std::string m_normal_style;

   /// The style used for fallthrough arcs.
   std::string m_fallthrough_style;

   /// The color used for function headers and key block.
   std::string m_normal_color;

   /// The color used for taken arcs and blocks.
   std::string m_taken_color;

   /// The color used for untaken arcs and blocks. 
   std::string m_untaken_color;

   /// The color used for partial blocks
   std::string m_partial_color;

   /// The color used for unkown arcs and blocks.
   std::string m_unknown_color;

   /// The default block background color.
   std::string m_default_block_fillcolor;

   /// The start and end block background color.
   std::string m_start_end_block_fillcolor;

   /// The taken block background color.
   std::string m_taken_block_fillcolor;

   /// The untaken block background color.
   std::string m_untaken_block_fillcolor;

   /// The partial block background color.
   std::string m_partial_block_fillcolor;

   /// The width used to draw normal lines and borders.
   double m_normal_width;

   /// The width used to draw bold borders.
   double m_bold_block_width; 

   /// The width used to draw bold lines.
   double m_bold_line_width;

}; // End of class Dot_creator 

#endif
