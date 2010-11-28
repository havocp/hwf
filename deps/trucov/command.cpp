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
///  @file command.cpp
///
///  @brief
///  Implements the commands of the Command class. 
///
///  Requirements Specification: 
///    < http://code.google.com/p/trucov/wiki/TrucovDesignDocumentation >
///
///////////////////////////////////////////////////////////////////////////////

//  INCLUDE FILE

#include "command.h"

//  USING STATEMENTS

using boost::bind;
using std::map;
using std::string;
using std::vector;
using std::setw;
using std::setprecision;
using std::fixed;
using std::cout;
using std::cerr;
using std::ofstream;
using std::endl;

// Initialize the instance pointer to null.
Command * Command::instance_ptr = NULL;

//  METHOD DEFINITIONS

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Initializes command data members. 
///////////////////////////////////////////////////////////////////////////////
Command::Command() 
{
    command_lookup[Config::Status] = bind(&Command::do_status, *this);
    command_lookup[Config::List] = bind(&Command::do_list, *this);
    command_lookup[Config::Report] = bind(&Command::do_report, *this);
    command_lookup[Config::Dot] = bind(&Command::do_dot, *this);
    command_lookup[Config::Dot_report] = bind(&Command::do_dot_report, *this);
    command_lookup[Config::Graph] = bind(&Command::do_render, *this);
    command_lookup[Config::Graph_report] = bind(&Command::do_render_report, *this);
    command_lookup[Config::All_report] = bind(&Command::do_all_report, *this);

} // End of Command default constructor.

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns a reference to the command class.
///
/// @return Reference to instance object.
///////////////////////////////////////////////////////////////////////////////
Command & Command::get_instance()
{
    if ( !Command::instance_ptr )
    {
        instance_ptr = new Command();
    }

    return *instance_ptr;

} // End of Command::get_instance()

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Prints a coverage summary for each function to stdout.
//////////////////////////////////////////////////////////////////////////////
int Command::do_status()
{
   Parser & parser = Parser::get_instance();

   // For all source files. 
   for ( map<string, Source_file>::const_iterator source_iter =
            parser.get_source_files().begin();
         source_iter != parser.get_source_files().end();
         source_iter++ )
   {
      const map<Source_file::Source_key, Record> & records =
         source_iter->second.m_records;

      // For all functions in a source file.
      for ( map<Source_file::Source_key, Record>::const_iterator record_it =
               records.begin();
            record_it != records.end();
            ++record_it )
      {
         const Record & rec = record_it->second;
         double den = rec.get_function_arc_total();
         
         // If function has no branch arcs
         if ( den == 0 )
         {
            // Output either 0% or 100%
            double percentage = rec.get_coverage_percentage();
            cout << setw(3) << fixed << setprecision(0) 
                    << percentage * 100 << "% " << rec.m_name_demangled 
                    << " no branches\n";
         }
         else
         {
            // Output function coverage information
            double num = rec.get_function_arc_taken();
            double percentage = rec.get_coverage_percentage();
            cout << setw(3) << fixed << setprecision(0) 
                    << percentage * 100 << "% " << rec.m_name_demangled 
                    << " (" << num << "/" << den << ") branches\n";
         } 
      }
   }  


   return 0;

} // End of Command::do_status(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Prints the name of each function specified in the selection to stdout.
//////////////////////////////////////////////////////////////////////////////
int Command::do_list()
{
   Parser & parser = Parser::get_instance();
  
   // For all source files. 
   for ( map<string, Source_file>::const_iterator source_iter =
            parser.get_source_files().begin();
         source_iter != parser.get_source_files().end();
         source_iter++ )
   {
      const map<Source_file::Source_key, Record> & records =
         source_iter->second.m_records;

      // For all functions in a source file.
      for ( map<Source_file::Source_key, Record>::const_iterator record_it =
               records.begin();
            record_it != records.end();
            record_it++ )
      {
         // Output function name.
         cout << record_it->second.m_name_demangled << "\n";
      }
   }

   return 0;

} // End of Command::do_list(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Creates a coverage file for each source file with the coverage
/// information. 
//////////////////////////////////////////////////////////////////////////////
int Command::do_report()
{
    Parser & ref_parser = Parser::get_instance();

    Coverage_creator cc;
    cc.generate_files( ref_parser );

    return 0;

} // End of Command::do_report(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Creates a dot report file with the coverage information of all source
/// files. 
//////////////////////////////////////////////////////////////////////////////
int Command::do_dot()
{
    Parser & ref_parser = Parser::get_instance();

    Dot_creator dc;
    dc.generate_file( ref_parser );

    return 0;

} // End of Command::dot_dot(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Creates a dot file for each source file with the coverage 
/// information.
//////////////////////////////////////////////////////////////////////////////
int Command::do_dot_report()
{
   Parser & ref_parser = Parser::get_instance();

   Dot_creator dc;
   dc.generate_files( ref_parser );

   return 0;

} // End of Command::do_dot_report(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Creates an image file for each source file.  
//////////////////////////////////////////////////////////////////////////////
int Command::do_render()
{
   Parser & ref_parser = Parser::get_instance();
   Config & config = Config::get_instance();

   Dot_creator dc;
   dc.set_render_type( config.get_flag_value( Config::Render_format ) );
   dc.generate_file( ref_parser );

   return 0;

} // End of Command::do_render(...)

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Creates an image file with coverage information of all source files. 
//////////////////////////////////////////////////////////////////////////////
int Command::do_render_report()
{
   Parser & ref_parser = Parser::get_instance();
   Config & config = Config::get_instance();

   Dot_creator dc;
   dc.set_render_type( config.get_flag_value( Config::Render_format ) );

   dc.generate_files( ref_parser );

   return 0;

} // End of Command::do_render_report(...)

/// @brief
/// Runs the report command the render command.
int Command::do_all_report()
{
   do_report();
   do_render_report();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief
/// Executes the command specified.
//////////////////////////////////////////////////////////////////////////////
bool Command::do_command( const string & command_name )
{
    Selector & selector = Selector::get_instance();
    Config & config = Config::get_instance();

    // Perform selection.
    selector.select( config.get_selection() );

    // If command is not found, then exit.
    if ( command_lookup.find( command_name ) == command_lookup.end() ) 
    {
        cerr << "ERROR:: " << command_name << " is not a valid command\n";
        return false;             
    }

    // Parse Gcno files.
    Parser & ref_parser = Parser::get_instance();
    if ( !ref_parser.parse_all() )
    {
        return false;
    }

    // Execute command.
    command_lookup[command_name]();

#ifdef DEBUGFLAG
   if (config.get_flag_value(Config::Secret_gui))
   {
      ofstream out(".__secret_gui__");
      if (!out.is_open())
      {
         cerr << "ERROR:: Secret gui file failed to open." << endl;
         return false;
      }

      ref_parser.gui_dump(out);
      out.close();
   }
#endif

    return true;

} // End of Command::do_command(...)

