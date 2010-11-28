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
///  @file command.h
///
///  @brief
///  Defines the main commands. 
///
///  @remarks
///  The command class is a singleton. 
///
///  Requirements Specification: < Null >
///
///////////////////////////////////////////////////////////////////////////////
#ifndef COMMAND_H
#define COMMAND_H

//  SYSTEM INCLUDES

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <string>
#include <sstream>

//  LOCAL INCLUDES  

#include "config.h"
#include "parser.h"
#include "dot_creator.h"
#include "coverage_creator.h"
#include "selector.h"

/// @class Parser
///
/// @brief
/// Parser gcno and gcda files.
///
/// @remarks
/// Class takes a name of a string when constructed, and then parses that
/// file when the parse method is called.
class Command
{
public:

//  PUBLIC METHODS

    /// @brief
    /// Returns a reference to the command class.
    ///
    /// @return Reference to instance object.
    static Command & get_instance();

    /// @brief
    /// Executes the command specified.
    bool do_command( const std::string & command_name );

private:

//  PRIVATE METHODS

    /// @brief
    /// Initializes command data members. 
    Command(); 

    /// @brief
    /// Prints a coverage summary for each function to stdout.
    int do_status();

    /// @brief
    /// Prints the name of each function specified in the selection to stdout.
    int do_list();

    /// @brief
    /// Creates a coverage file for each source file with the coverage
    /// information. 
    int do_report();

    /// @brief
    /// Creates a dot file for each source file with the coverage 
    /// information.
    int do_dot();

    /// @brief
    /// Creates a dot report file with the coverage information of all source
    /// files. 
    int do_dot_report();

    /// @brief
    /// Creates an image file for each source file.  
    int do_render();

    /// @brief
    /// Creates an image file with coverage information of all source files. 
    int do_render_report();

    /// @brief
    /// Runs the report command the render command.
    int do_all_report();

//  PRIVATE MEMBERS

    /// Pointer to the instance.
    static Command * instance_ptr;

    /// Lookup table for commands.
    std::map<std::string, boost::function<int ()> > 
        command_lookup; 

}; // End of class Command

#endif

