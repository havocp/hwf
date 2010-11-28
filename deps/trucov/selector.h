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
///  @file selector.h
///
///  @brief
///  Defines the Selector class  
///
///  @remarks
///  The Selector class will select the functions to store data on.
///
///  Requirements Specification:
///    < http://code.google.com/p/trucov/wiki/SRS > 
///
///  Design Description: 
///     < http://code.google.com/p/trucov/wiki/TrucovDesignDocumentation >
///////////////////////////////////////////////////////////////////////////////
#ifndef SELECTOR_H
#define SELECTOR_H

//  SYSTEM INCLUDES

#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

#include <boost/bind.hpp>
#include <boost/regex.hpp>

//  LOCAL INCLUDES

#include "config.h"
#include "tru_utility.h"

/// @brief 
/// Performs checks on functions and source files to see if they were selected
class Selector
{
public:

//  PUBLIC METHODS

    /// @brief
    /// Populates the selection data from the config
    ///
    /// @param config A reference to the config
    void select( const std::vector<std::string> & selection_list );

    /// @brief
    /// Returns a reference to the singleton
    ///
    /// @return Selector & 
    static Selector & get_instance();

    /// @brief
    /// Returns if a function or source was selected or not
    ///
    /// @param source_name The source file name
    /// @param function_signature The name of the function
    ///
    /// @return bool
    bool is_selected( const std::string & source_name,
        const std::string & function_signature ); 

private:

//  PRIVATE METHODS

    /// @brief
    /// Initializes selection flags
    Selector();
    
    /// @brief 
    /// Declared, but not implemented
    Selector( const Selector & );
    
    /// @brief
    /// Declared, but not implemented 
    ~Selector();

    /// @brief
    /// Returns if the string can be matched to an expression
    ///
    /// @param input The string to be matched to a regex
    /// @param expression The regex expression
    ///
    /// @return bool 
    bool check_in_selection( const std::string & input ); 

    /// @brief
    /// Parsers the function out from the function signature.
    ///
    /// @param function_signature The demangled signature of the function.
    ///
    /// @remarks
    /// This is a helper method for is_selected()
    std::string parse_function_name( const std::string & function_signature );

//  PRIVATE MEMBERS

    /// Determines if all functions were selected or not
    bool is_all_selected;

    /// Determines if all regex's must be matched, or just 1
    bool is_and_match;

    /// Determines if full match is selected or partial match
    bool is_full_match;

    /// Determines is to match the signature or the name of a function
    bool is_signature_match;

    /// Lists all functions selected
    std::vector< boost::regex > regex_selections;

    /// Pointer to the Selector instance
    static Selector * instance_ptr; 

}; // End of class Selector 

#endif 

