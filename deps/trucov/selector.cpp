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
///  @file selector.cpp
///
///  @brief
///  Implements the Selector class methods
///
///  Requirements Specification:
///     < http://code.google.com/p/trucov/wiki/SRS > 
///
///  Design Description: 
///     < http://code.google.com/p/trucov/wiki/TrucovDesignDocumentation >
///////////////////////////////////////////////////////////////////////////////

//  HEADER FILE

#include "selector.h"

//  USING STATEMENTS

using std::cerr;
using std::string;
using std::vector;
using std::find_if;

using boost::bind;
using boost::function;
using boost::regex;
using boost::regex_match;
using boost::match_flag_type;
using boost::match_default;
using boost::match_partial;

//  INITIALIZE INSTANCE 

Selector * Selector::instance_ptr = NULL;

//  METHOD DEFINTIIONS

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Initializes selector flags
//////////////////////////////////////////////////////////////////////////////
Selector::Selector()
{
    is_all_selected = true;
    is_full_match = true;
    is_signature_match = false;

} // End of Selector Constructor

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns a reference to the singleton
///
/// @return Selector & 
//////////////////////////////////////////////////////////////////////////////
Selector & Selector::get_instance()
{
    // Initialize instance if not initialize yet
    if ( instance_ptr == NULL )
    {
        instance_ptr = new Selector();
    } 

    return * instance_ptr;

} // End of Selector::get_instance()

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Populates the selection data from the config
///
/// @param config A reference to the config
//////////////////////////////////////////////////////////////////////////////
void Selector::select( const vector<string> & selection_list )
{
    // If selection list is empty, then nothing default to all selected 
    is_all_selected = ( selection_list.size() == 0 );

    // Clear selections
    regex_selections.clear();

    // Get full, and, and signature flags
    Config & config = Config::get_instance();
    is_full_match = config.get_flag_value(Config::Full_match);
    is_and_match = config.get_flag_value(Config::And);
    is_signature_match = config.get_flag_value(Config::Signature_match);

    // Populate regex selection list
    for (unsigned int i = 0; i < selection_list.size(); i++)
    {
        if ( is_full_match )
        {
            regex_selections.push_back( regex( selection_list[i] ) );
        }
        else
        {  
            regex_selections.push_back( regex( ".*" + selection_list[i] + ".*" ) );
        }
    }

#ifdef DEBUGFLAG
    // Verbose for debugging 
    if ( config.get_flag_value(Config::Debug) )
    {
        cerr << "Selected Elements = { ";
        for ( unsigned int i = 0; i < regex_selections.size(); i++ )
        {
            cerr << regex_selections[i] << " ";
        }

        cerr << "}\nSelect: full_match = " << is_full_match
             << "\nSelect: and_match = " << is_and_match
             << "\nSelect: signature_match = " << is_signature_match << "\n";        
    }
#endif

} // End of Selector::select( ... )

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns if a function was selected or not
///
/// @param source_name The source file name
/// @param function_signature The name of the function
///
/// @return bool
//////////////////////////////////////////////////////////////////////////////
bool Selector::is_selected( const std::string & source_name,
    const std::string & function_signature )
{
    // Instantiate utility class pointer
    Tru_utility * ptr_utility = Tru_utility::get_instance();

    // If everything is selected or if function is selected then return true.
    if ( is_all_selected 
        || check_in_selection( ptr_utility->get_filename( source_name ) ) )
    {
        return true;
    }

    // If match by function signature, then parse the function name out
    // first and then attempt to match.
    if ( !is_signature_match )
    {
        // Get function name.
        string function_name = parse_function_name( function_signature );

        return check_in_selection( function_name );
    }
    else // Otherwise, match by function signature. 
    {
       return check_in_selection( function_signature );
    }

} // End of Selector::is_selected( ... )

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns if the string can be matched to an expression
///
/// @param input The string to be matched to a regex
/// @param expression The regex expression
///
/// @return bool 
//////////////////////////////////////////////////////////////////////////////
bool Selector::check_in_selection( const std::string & input )
{
    // The function pointer type of the overloaded match_regex we will use
    // for matching.
    typedef bool (*Regex_match_ptr)( const string &, const regex &,
        match_flag_type);

    // Set match flag to full match or partial match, whatever was selected.
    match_flag_type match_type = match_default;
  
    if ( !is_and_match )
    {  
        // Check regex_match(input, ith expression, match_type) matches any 
        // of the expressions in selection list. We must cast regex_match to the 
        // overloaded function pointer type we are using.
        return ( find_if( regex_selections.begin(), regex_selections.end(), 
            bind( static_cast<Regex_match_ptr>( regex_match ), input, _1, 
                match_type ) ) != regex_selections.end() );
    }
    else
    {
        return ( count_if( regex_selections.begin(), regex_selections.end(),
            bind( static_cast<Regex_match_ptr>( regex_match ), input, _1,
                match_type ) ) == regex_selections.size() ); 
    }

} // End of Selector::check_in_selection( ... )

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Parsers the function out from the function signature.
///
/// @param function_signature The demangled signature of the function.
///
/// @remarks
/// This is a helper method for is_selected()
//////////////////////////////////////////////////////////////////////////////
string Selector::parse_function_name( const string & function_signature )
{
    // The function name ends before the first '(', and starts after the 
    // first ':' before the '('.
    unsigned int name_end = function_signature.find_first_of( '(' ) - 1;
    unsigned int name_start = 
        function_signature.rfind( ':', name_end ) + 1;

    // Return the name, which is between start and end.  
    return function_signature.substr( name_start, name_end - name_start + 1 );

} // End of Selector::parse_function_name( ... )


