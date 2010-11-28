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
///  @file revision_script_manager.cpp 
///
///  @brief
///  Implements the methods of the Revision_script_manager class. 
///////////////////////////////////////////////////////////////////////////////

//  HEADER FILE

#include "revision_script_manager.h"

//  USING STATEMENTS

using std::string;

//  FUNCTION DEFINITIONS

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Initialize the revision script pointer to null. 
///////////////////////////////////////////////////////////////////////////////
Revision_script_manager::Revision_script_manager( 
   const string & revision_script_path )
{
    // Save revision script path.
    m_revision_script_path = revision_script_path;

    // Empty string is not valid.
    m_revision_valid = ( m_revision_script_path.size() > 0 );
    
} // End of default constructor

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Close pipe file pointer if it's still open. 
///////////////////////////////////////////////////////////////////////////////
Revision_script_manager::~Revision_script_manager()
{

} // End of default destructor

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns if revision script is valid or not.
///
/// @return valid script (true), invalid (false).
///////////////////////////////////////////////////////////////////////////////
bool Revision_script_manager::is_valid() const
{
    return m_revision_valid;

} // End of Revision_script_manager::is_valid()

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns the revision number for a source file.
///
/// @param file_path The path to the source file to get the revision 
/// number.
///
/// @return The revision number of the file in string form. 
///////////////////////////////////////////////////////////////////////////////
std::string Revision_script_manager::get_revision_number( 
    const std::string & file_path )
{
    string revision_number;

    if ( is_valid() )
    {
        Tru_utility * helper = Tru_utility::get_instance();  
        
        revision_number = 
            helper->execute_pipe(m_revision_script_path, 
                file_path + " 2> null");
    }

    return revision_number;

} // End of Revision_script_manager::get_revision_number(...)

