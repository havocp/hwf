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
///  @file gcda_actors.h 
///
///  @brief
///  Defines the Data structure for GCDA Parsing.
///  Implements the Actors (Semantic Actions) for the GCDA grammar.
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
#ifndef GCDA_ACTORS_H
#define GCDA_ACTORS_H

// SYSTEM INCLUDES

#include <string>
#include <iostream>

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

// LOCAL INCLUDES

#include "parser_builder.h"
#include "record.h"

// DATA DEFINITIONS

/// @brief
/// Contains all data fields necessary to parse the GCNO File
struct Parsing_data_gcda
{
    /// @brief
    /// Intializes the references to data records, and dump information.
    ///
    /// @param pd_ref Reference to data records.
    /// @param is_dump_ref Reference to dump flag.
    /// @param dump_file_ref Reference to dump file output stream.
    Parsing_data_gcda( 
        Parser_builder & parser_builder_ref, 
        const bool is_dump_ref,
        std::ofstream & dump_file_ref )
        : parser_builder( parser_builder_ref ), 
          is_dump( is_dump_ref ), 
          dump_file( dump_file_ref )
    {
        // void
    }

    // Records Data
    unsigned int version;
    unsigned int stamp;
    unsigned int rLength;
    unsigned int rIdent;
    unsigned int rChecksum;

    // Annouce Function data
    uint64_t aCount;
    unsigned int aLength;

    // Object Summary data
    unsigned int oChecksum;
    unsigned int oLength;
    unsigned int oCounts;
    unsigned int oRuns;
    unsigned int oSumall;
    uint64_t oRunmax;
    uint64_t oSummax;

    // Program Summary data
    unsigned int pChecksum;
    unsigned int pLength;
    unsigned int pCounts;
    unsigned int pRuns;
    unsigned int pSumall;
    uint64_t pRunmax;
    uint64_t pSummax;

    /// Reference to the parser builder to pass the data to.
    Parser_builder & parser_builder;

    /// Determines if parser is dumping to data read to dump files.
    const bool is_dump;
   
    /// The dump file output stream. 
    std::ofstream & dump_file;

}; // end of struct Parsing_data_gcda

///////////////////////////////////////////////////////////////////////////////
// GCDA GRAMMAR ACTIONS
///////////////////////////////////////////////////////////////////////////////

/// @brief
/// Handles data storage / dump of version and stamp of gcda file.
struct gcda_action_gcdafile
{
    template<typename IteratorT>
    void act(Parsing_data_gcda & pd, IteratorT const & first_,
             IteratorT const & last_) const
    {
#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << "Version: " << pd.version
                         << " Stamp: "  << pd.stamp << "\n";
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
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda, gcda_action_gcdafile>
       do_gcdafile(Parsing_data_gcda & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda, gcda_action_gcdafile>(pd);
}

/// @brief
/// Handles data storage / dump of AnnounceFunction
struct gcda_action_AnnounceFunction
{
    template<typename IteratorT>
    void act(Parsing_data_gcda & pd, IteratorT const & first_,
             IteratorT const & last_) const
    {
#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << "rLength: " << pd.rLength
                         << " rIdent: " << pd.rIdent
                         << " rChecksum: " << pd.rChecksum << "\n";
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle rLength, rIdent, rChecksum
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda, gcda_action_AnnounceFunction>
       do_AnnounceFunction(Parsing_data_gcda & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda,
                           gcda_action_AnnounceFunction>(pd);
}

/// @brief
/// Handles data storage / dump of aLength
struct gcda_action_aLength
{
    template<typename IteratorT>
    void act(Parsing_data_gcda & pd, IteratorT const & first_,
             IteratorT const & last_) const
    {

#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << "   aLength: " << pd.aLength << "\n";
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle aLength
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda, gcda_action_aLength>
       do_aLength(Parsing_data_gcda & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda,
                           gcda_action_aLength>( pd );
}

/// @brief
/// Handles data storage / dump of aCount
struct gcda_action_aCount
{
    template<typename IteratorT>
    void act(Parsing_data_gcda & pd, IteratorT const & first_,
             IteratorT const & last_) const
    {
#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << "     aCount: " << pd.aCount << "\n";
        }
#endif
        // Store arc count
        pd.parser_builder.store_count( pd.rIdent, pd.rChecksum, pd.aCount );
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle aCount
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda, gcda_action_aCount>
       do_aCount(Parsing_data_gcda & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda,
                           gcda_action_aCount>(pd);
}

/// @brief
/// Handles data storage / dump of Object Summary
struct gcda_action_ObjectSummary
{
    template<typename IteratorT>
    void act(Parsing_data_gcda & pd, IteratorT const & first_,
             IteratorT const & last_) const
    {
#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << " oLength: " << pd.oLength << "\n"
             << "   oChecksum: " << pd.oChecksum << "\n"
             << "   oCounts: " << pd.oCounts << "\n"
             << "   oRuns: " << pd.oRuns << "\n"
             << "   oSumall: " << pd.oSumall << "\n"
             << "   oRunmax: " << pd.oRunmax << "\n"
             << "   oSummax: " << pd.oSummax << "\n";
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle oLength, oChecksum, oCounts, oRuns, oSumall,
/// oRunmax, oSummax.
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda, gcda_action_ObjectSummary>
       do_ObjectSummary(Parsing_data_gcda & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda,
                           gcda_action_ObjectSummary>(pd);
}

/// @brief
/// Handles data storage / dump of Program Summary
struct gcda_action_ProgramSummary
{
    template<typename IteratorT>
    void act(Parsing_data_gcda & pd, IteratorT const & first_,
             IteratorT const & last_) const
    {
#ifdef DEBUGFLAG
        if (pd.is_dump)
        {
            pd.dump_file << " pLength: " << pd.pLength << "\n"
               << "   pChecksum: " << pd.pChecksum << "\n"
               << "   pCounts: " << pd.pCounts << "\n"
               << "   pRuns: " << pd.pRuns << "\n"
               << "   pSumall: " << pd.pSumall << "\n"
               << "   pRunmax: " << pd.pRunmax << "\n"
               << "   pSummax: " << pd.pSummax << "\n";
        }
#endif
    }
};

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Returns Functor to handle pLength, pChecksum, pCounts, pRuns, pSumall,
/// pRunmax, pSummax
///
/// @param pd Parsing Data of the grammar
///
/// @return ref_value_actor to perform functor
///////////////////////////////////////////////////////////////////////////////
inline SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda, gcda_action_ProgramSummary>
do_ProgramSummary(Parsing_data_gcda & pd)
{
    return SPIRIT_NAMESPACE::ref_value_actor<Parsing_data_gcda,
                           gcda_action_ProgramSummary>(pd);
}

#endif

