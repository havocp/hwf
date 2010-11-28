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
///  @file revision_script_manager.h 
///
///  @brief
///  Implements ability to pull revision information from the source files.
///
///  @remarks
///  Defines the Revision_script_manager class.
///////////////////////////////////////////////////////////////////////////////
#ifndef  REVISION_SCRIPT_MANAGER_H
#define  REVISION_SCRIPT_MANAGER_H

//  SYSTEM INCLUDES

#include <iostream>
#include <string>
#include <algorithm>

//  LOCAL INCLUDES

#include "tru_utility.h"

/// @class Revision_script_manager
///
/// @brief
/// Pulls revision information from source files. 
class Revision_script_manager
{
public:

//  PUBLIC METHODS

    /// @brief
    /// Initialize the revision script pointer to null. 
    Revision_script_manager( const std::string & m_revision_script_path );

    /// @brief
    /// Close pipe file pointer if it's still open. 
    ~Revision_script_manager();

    /// @brief
    /// Returns if revision script is valid or not.
    ///
    /// @return valid script (true), invalid (false).
    bool is_valid() const;

    /// @brief
    /// Returns the revision number for a source file.
    ///
    /// @param file_path The path to the source file to get the revision 
    /// number.
    ///
    /// @return The revision number of the file in string form. 
    std::string get_revision_number( const std::string & file_path );

private:

//  PRIVATE MEMBERS 

    /// Path to the revision script. 
    std::string m_revision_script_path;

    /// Determines whether or not the script specified was valid or not.
    bool m_revision_valid;

}; // End of class Revision_script_manager

#endif

