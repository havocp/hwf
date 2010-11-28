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
///  @file gcno_actors.h 
///
///  @brief
///  Defines the Data structure for GCNO Parsing.
///  Implements the Actors (Semantic Actions) for the GCNO grammar.
///
///  @remarks
///  Actors are defined in pairs (functor & function). Refer to the function
///  name in spirit semantic blocks.
///
///  Requirements Specification:
///     < http://code.google.com/p/trucov/wiki/SRS >
///
///  Design Description: 
///     < http://code.google.com/p/trucov/wiki/TrucovDesignDocumentation >
///////////////////////////////////////////////////////////////////////////////
#ifndef GCNO_ACTORS_H
#define GCNO_ACTORS_H

//  SYSTEM INCLUDES

#include <string>
#include <iostream>
#include <fstream>

#include <boost/version.hpp>

#if BOOST_VERSION < NEW_SPIRIT_VERSION
   #define SPIRIT_NAMESPACE boost::spirit
   #include <boost/spirit/actor.hpp>
   #include <boost/spirit/core.hpp>
#else
   #define SPIRIT_NAMESPACE boost::spirit::classic
   #include <boost/spirit/include/classic_actor.hpp>
   #include <boost/spirit/include/classic_core.hpp>
#endif

//  LOCAL INCLUDES

#include "parser_builder.h"
#include "record.h"

//  DATA DEFINITIONS

/// @brief Contains all data fields necessary to parse the GCNO File
struct Parsing_data_gcno
{
    Parsing_data_gcno(
        Parser_builder & parser_builder_ref,
        const bool is_dump_ref, 
        std::ofstream & dump_file_ref)
        : parser_builder(parser_builder_ref), 
          is_dump(is_dump_ref),
          dump_file(dump_file_ref)
    {
        // void
    }

    // Records Data
    unsigned int version;
    unsigned int stamp;
    unsigned int rIdent;
    unsigned int rChecksum;
    unsigned int rLength;
    unsigned int blocks;
    std::string rName;
    std::string rSource;
    unsigned int rLineno;

    // Blocks Data
    unsigned int bLength;
    unsigned int bFlags;
    unsigned int bIteration;

    // Arcs Data
    unsigned int aBlockno;
    unsigned int aDestBlock;
    unsigned int aLength;
    unsigned int aFlags;

    // Lines Data
    unsigned int lLength;
    unsigned int lBlockno;
    unsigned int lLineno;
    std::string lName;

    /// A reference to the parser builder to construct the data structure.
    Parser_builder & parser_builder;

    /// Determines if dump output should be created.
    const bool is_dump;

    /// Dump file stream.
    std::ofstream & dump_file;
};

//  GCNO GRAMMAR ACTIONS

/// @brief
/// Handles data storage / dump of version and stamp of gcno file.
struct gcno_action_gcnofile
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first_,
             IteratorT const & last_) const
    {
#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << "Version: " << pd.version << "\n"
                      << "Stamp:   " << pd.stamp   << "\n";
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle version, stamp
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_gcnofile>
       do_gcnofile(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_gcnofile>(pd);
}

/// @brief
/// Handles data storage / dump of rLength, rIdent, rChecksum, rName, rSource,
/// and rLineno
struct gcno_action_AnnounceFunction
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first,
             IteratorT const & last) const
    {
#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << "AnnounceFunction: "
                 << "rLength= " << pd.rLength
                 << " rIdent= " << pd.rIdent
                 << " rChecksum= " << pd.rChecksum << "\n";
            pd.dump_file << "                  rName= " << pd.rName << std::endl
                 << "                  rSource= " << pd.rSource << std::endl
                 << "                  rLineno= " << pd.rLineno << std::endl;
        }
#endif
        pd.parser_builder.store_record( pd.rIdent, pd.rChecksum, pd.rSource, 
            pd.rName, pd.rLineno );
   }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to Handle AnnounceFunction data.
///
/// @param pd Parsing Data of the grammar.
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_AnnounceFunction>
       do_AnnounceFunction(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno,
                           gcno_action_AnnounceFunction>(pd);
}

/// @brief
/// Handles data storage / dump of bLength
struct gcno_action_BasicBlocks
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first,
             IteratorT const & last) const
    {
#ifdef DEBUGFLAG
        if ( pd.is_dump )
        {
            pd.dump_file << "BasicBlocks:  bLength= " << pd.bLength << std::endl;
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle bLength.
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_BasicBlocks>
       do_BasicBlocks(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_BasicBlocks>(pd);
}

/// @brief
/// Handles data storage / dump of bFlags.
struct gcno_action_bFlags
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first,
             IteratorT const & last) const
    {
#ifdef DEBUGFLAG
        if ( pd.is_dump )
        {

        pd.dump_file << "blocks=" << pd.blocks << "   bIteratin" << pd.bIteration <<  "   bFlags= " << pd.bFlags << std::endl;

        }
#endif
        
        pd.parser_builder.store_blocks( pd.bLength, pd.bFlags, pd.bIteration );
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle bFlags
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_bFlags>
       do_bFlags(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_bFlags>(pd);
}

/// @brief
/// Handles data storage / dump of aLength and aBlockno.
struct gcno_action_Arcs
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first,
             IteratorT const & last) const
    {   
#ifdef DEBUGFLAG
        if ( pd.is_dump )
        { 
            pd.dump_file << "Arcs: aLength= " << pd.aLength << " aBlockno= "
             << pd.aBlockno << std::endl;
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to Handle aLength and aBlockno
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_Arcs>
       do_Arcs(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_Arcs>(pd);
}

/// @brief
/// Handles data storage / dump of destblock and aFlags
struct gcno_action_Arc
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first,
             IteratorT const & last) const
    {
#ifdef DEBUGFLAG
        if ( pd.is_dump )
        {
            pd.dump_file << "      destblock= " << pd.aDestBlock << " aFlags= "
             << pd.aFlags << std::endl;
        }
#endif

        pd.parser_builder.store_arcs( pd.aBlockno, pd.aDestBlock, pd.aFlags ); 
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to Handle Arc data
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_Arc>
       do_Arc(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_Arc>(pd);
}

/// @brief
/// Handles data storage / dump of lLength and lBlockno
struct gcno_action_Lines
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first,
             IteratorT const & last) const
    {
#ifdef DEBUGFLAG
        if ( pd.is_dump )
        {
            pd.dump_file << "  Lines:"
                         << " lLength= " << pd.lLength
                         << " lBlockno= " << pd.lBlockno << "\n";
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle Lines data
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_Lines>
       do_Lines(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_Lines>(pd);
}

/// @brief
/// Handles data storage / dump of lName
struct gcno_action_lName
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first,
             IteratorT const & last) const
    {
#ifdef DEBUGFLAG
        if ( pd.is_dump )
        {
            pd.dump_file << "         lname= " << pd.lName << "\n";
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle lName
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_lName>
       do_lName(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_lName>(pd);
}

/// @brief
/// Handles data storage / dump of lLineno
struct gcno_action_lLineno
{
    template<typename IteratorT>
    void act(Parsing_data_gcno & pd, IteratorT const & first,
             IteratorT const & last) const
    {
#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << "         lLineno= " << pd.lLineno << "\n";
        }
#endif

        pd.parser_builder.store_line_number( pd.lBlockno, pd.lLineno,
            pd.lName );
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Return Functor to handle lLineno
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_lLineno>
       do_lLineno(Parsing_data_gcno & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcno, gcno_action_lLineno>(pd);
}

#endif

