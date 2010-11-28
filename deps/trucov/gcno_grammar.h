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
///  @file gcno_grammar.h
///
///  @brief
///  Defines The GCNO Grammar for the Spirit Parsing Framework
///
///  @remarks
///  The Gcno_grammar struct shall be handled by Spirit Parsing only.
///
///  Requirements Specification:
///     < http://code.google.com/p/trucov/wiki/SRS >
///
///  Design Description: 
///     < http://code.google.com/p/trucov/wiki/TrucovDesignDocumentation >
///////////////////////////////////////////////////////////////////////////////
#ifndef GCNO_GRAMMAR_H
#define GCNO_GRAMMAR_H

//  SYSTEM INCLUDES

#include <fstream>
#include <iostream>
#include <exception>
#include <map>
#include <inttypes.h>

#include <boost/variant.hpp>
#include <boost/ref.hpp>

#include <boost/version.hpp>

#if BOOST_VERSION < NEW_SPIRIT_VERSION
   #define SPIRIT_NAMESPACE boost::spirit
   #include <boost/spirit/core.hpp>
   #include <boost/spirit/tree/ast.hpp>
   #include <boost/spirit/attribute.hpp>
   #include <boost/spirit/phoenix.hpp>
   #include <boost/spirit/utility.hpp>
   #include <boost/spirit/dynamic.hpp>
#else
   #define SPIRIT_NAMESPACE boost::spirit::classic
   #include <boost/spirit/include/classic_core.hpp>
   #include <boost/spirit/include/classic_ast.hpp>
   #include <boost/spirit/include/classic_attribute.hpp>
   #include <boost/spirit/include/phoenix1.hpp>
   #include <boost/spirit/include/classic_utility.hpp>
   #include <boost/spirit/include/classic_dynamic.hpp>
#endif

//  LOCAL INCLUDES

#include "gcno_actors.h"
#include "prims.h"
#include "record.h"

//  USING STATEMENTS

/// Defines the Gcno grammar for the Spirit Parsing Framework.
/// This grammar may be invoked with spirit by using the parse function.
struct Gcno_grammar : public SPIRIT_NAMESPACE::grammar<Gcno_grammar>
{
    //  PUBLIC DATA FIELDS
public:

    /// @brief
    /// Defines the grammar for the gcno File
    template<typename ScannerT>
    struct definition
    {
        /// @brief
        /// Spirit calls this constructor with a reference to the grammar
        /// object this definition is contained in.
        ///
        /// @param s A reference to definition object; needed for data access.
        definition(Gcno_grammar const & s)
        {
            //  TOKEN DEFINITIONS

            ZERO_32      = TOKEN32(0x00000000);
            MAGIC        = TOKEN32(0x67636E6F);
            TAG_FUNCTION = TOKEN32(0x01000000);
            TAG_BLOCKS   = TOKEN32(0x01410000);
            TAG_ARCS     = TOKEN32(0x01430000);
            TAG_LINES    = TOKEN32(0x01450000);

            //  ALIAS DEFINITIONS

            version   = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.version, global_parsed_int32)];
            stamp     = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.stamp, global_parsed_int32)];
            rLength   = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.rLength, global_parsed_int32)];
            rIdent    = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.rIdent, global_parsed_int32)];
            rChecksum = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.rChecksum, global_parsed_int32)];
            rName     = STRING [SPIRIT_NAMESPACE::assign_a(s.pd_ref.rName, global_parsed_string)];
            rSource   = STRING [SPIRIT_NAMESPACE::assign_a(s.pd_ref.rSource, global_parsed_string)];
            rLineno   = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.rLineno, global_parsed_int32)];
            bLength   = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.bLength, global_parsed_int32)]
                                   [SPIRIT_NAMESPACE::assign_a(s.pd_ref.blocks, global_parsed_int32)]
                                   [phoenix::var(s.pd_ref.blocks) -= 1];
            bFlags    = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.bFlags, global_parsed_int32)];
            aLength   = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.aLength, global_parsed_int32)]
                                   [phoenix::var(s.pd_ref.aLength) /= 2];
            aBlockno  = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.aBlockno, global_parsed_int32)];
            aDestBlock = INT32 [SPIRIT_NAMESPACE::assign_a(s.pd_ref.aDestBlock, global_parsed_int32)];
            aFlags    = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.aFlags, global_parsed_int32)];
            lLength   = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.lLength, global_parsed_int32)];
            lBlockno  = INT32  [SPIRIT_NAMESPACE::assign_a(s.pd_ref.lBlockno, global_parsed_int32)];
            lLineno   = NONZERO[SPIRIT_NAMESPACE::assign_a(s.pd_ref.lLineno, global_parsed_int32)];
            lName     = STRING [SPIRIT_NAMESPACE::assign_a(s.pd_ref.lName, global_parsed_string)];

            //  GRAMMAR RULE DEFINITIONS

            GcnoFile =
                    MAGIC >> version >> stamp [ do_gcnofile(s.pd_ref) ] >>
                    *( Record )
                ;
            Record   =
                AnnounceFunction [ do_AnnounceFunction(s.pd_ref) ] >>
                BasicBlocks >> 
                SPIRIT_NAMESPACE::repeat_p(boost::ref(s.pd_ref.blocks)) 
                    [ Arcs ] >>
                *Lines
                ;
            AnnounceFunction =
                TAG_FUNCTION >> rLength >> rIdent >> rChecksum  >> rName >>
                rSource >> rLineno
                ;
            BasicBlocks =
                TAG_BLOCKS >>  bLength [ do_BasicBlocks(s.pd_ref)     ]
                    [ phoenix::var(s.pd_ref.bIteration) = 0 ] >>
                SPIRIT_NAMESPACE::repeat_p(boost::ref(s.pd_ref.bLength)) [
                    bFlags [ do_bFlags(s.pd_ref)           ]
                           [ phoenix::var(s.pd_ref.bIteration) += 1 ]
                ]
                ;
            Arcs =
                TAG_ARCS >> aLength >>  aBlockno [ do_Arcs(s.pd_ref) ] >>
                SPIRIT_NAMESPACE::repeat_p(boost::ref(s.pd_ref.aLength)) [
                    Arc [ do_Arc(s.pd_ref) ]
                ]
                ;
            Arc =
                aDestBlock >> aFlags
                ;
            Lines =
                TAG_LINES >> lLength >> lBlockno [ do_Lines(s.pd_ref) ] >>
                *Line >> ZERO_32 >> ((ZERO_32 >> ZERO_32) | ZERO_32)
                ;
            Line  =
                lLineno [ do_lLineno(boost::ref(s.pd_ref)) ]
                | ZERO_32 >> lName  >>
                  lLineno[ do_lName(boost::ref(s.pd_ref)) ]
                         [ do_lLineno(boost::ref(s.pd_ref)) ]
                | ZERO_32 >> lName [ do_lName(boost::ref(s.pd_ref)) ] 
                ;

        } // end of constructor of defintion

        //  TOKENS

        SPIRIT_NAMESPACE::rule<ScannerT> ZERO_32;
        SPIRIT_NAMESPACE::rule<ScannerT> MAGIC;
        SPIRIT_NAMESPACE::rule<ScannerT> TAG_FUNCTION;
        SPIRIT_NAMESPACE::rule<ScannerT> TAG_BLOCKS;
        SPIRIT_NAMESPACE::rule<ScannerT> TAG_ARCS;
        SPIRIT_NAMESPACE::rule<ScannerT> TAG_LINES;

        //  ALIASES

        SPIRIT_NAMESPACE::rule<ScannerT> version;
        SPIRIT_NAMESPACE::rule<ScannerT> stamp;
        SPIRIT_NAMESPACE::rule<ScannerT> rLength;
        SPIRIT_NAMESPACE::rule<ScannerT> rIdent;
        SPIRIT_NAMESPACE::rule<ScannerT> rChecksum;
        SPIRIT_NAMESPACE::rule<ScannerT> rName;
        SPIRIT_NAMESPACE::rule<ScannerT> rSource;
        SPIRIT_NAMESPACE::rule<ScannerT> rLineno;
        SPIRIT_NAMESPACE::rule<ScannerT> bLength;
        SPIRIT_NAMESPACE::rule<ScannerT> bFlags;
        SPIRIT_NAMESPACE::rule<ScannerT> aLength;
        SPIRIT_NAMESPACE::rule<ScannerT> aBlockno;
        SPIRIT_NAMESPACE::rule<ScannerT> aDestBlock;
        SPIRIT_NAMESPACE::rule<ScannerT> aFlags;
        SPIRIT_NAMESPACE::rule<ScannerT> lLength;
        SPIRIT_NAMESPACE::rule<ScannerT> lBlockno;
        SPIRIT_NAMESPACE::rule<ScannerT> lLineno;
        SPIRIT_NAMESPACE::rule<ScannerT> lName;

        //  GRAMMAR RULES

        SPIRIT_NAMESPACE::rule<ScannerT> GcnoFile;
        SPIRIT_NAMESPACE::rule<ScannerT> Record;
        SPIRIT_NAMESPACE::rule<ScannerT> AnnounceFunction;
        SPIRIT_NAMESPACE::rule<ScannerT> BasicBlocks;
        SPIRIT_NAMESPACE::rule<ScannerT> Arcs;
        SPIRIT_NAMESPACE::rule<ScannerT> Arc;
        SPIRIT_NAMESPACE::rule<ScannerT> Lines;
        SPIRIT_NAMESPACE::rule<ScannerT> Line;

        /// @brief
        /// Defines the start rule for the GCNO Grammar definition. This 
        /// method is used by spirit.
        ///
        /// @return The top GCNO grammar rule
        SPIRIT_NAMESPACE::rule<ScannerT> const & start() const
        {
            return GcnoFile;
        }

    }; // end of struct definition

    //  PUBLIC METHODS

    /// @brief
    /// Initializes a reference to the internal parsing data for spirit and
    /// the error messages.
    /// 
    /// @param records Reference to records map that will be populated by
    /// the parse method.
    Gcno_grammar(Parser_builder & parser_builder, 
    const bool is_dump_ref, std::ofstream & dump_file_ref) 
    : parsing_data(parser_builder, is_dump_ref, dump_file_ref), pd_ref(parsing_data)
    {
        // Initialize error messages
        error_msg_tag_blocks   =
            "ERROR: Failed to match token TAG_BLOCKS Failed.";
        error_msg_tag_arcs     =
            "ERROR: Failed to match token TAG_ARCS Failed.";
    }

    //  PRIVATE DATA FIELDS
private:

    /// The private temporary internal parsing data of the file
    Parsing_data_gcno parsing_data;

    ///  A private error message for the token TAG_BLOCKS
    std::string error_msg_tag_blocks;

    ///  A private error message for the token TAG_ARCS
    std::string error_msg_tag_arcs;

    ///  A public reference to the Parsing Data that spirit will use
    Parsing_data_gcno & pd_ref;

}; // end of struct Gcno_grammar

#endif

