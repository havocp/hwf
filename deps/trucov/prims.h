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
///  @file prims.h
///
///  @brief 
///  Defines the primitives used by Trucov Parsing to read Raw Byte Streams
///
///  @remarks
///  Defines INT32 primitive: Reads int32 and returns value
///  Defines TOKEN32 primitive: Matches specific int32 value
///  Defines STRING primitive: Reads an arbitrary length string
///  Defines NONZERO pirmitive: Reads nonzero int32 and returns value
///
///  Requirements Specification: 
///       < http://code.google.com/p/trucov/wiki/SRS > 
///
///  Design Description: 
///       < http://code.google.com/p/trucov/wiki/TrucovDesignDocumentation >
///////////////////////////////////////////////////////////////////////////////
#ifndef PRIMS_H
#define PRIMS_H

//  SYSTEM INCLUDES

#include <iostream>
#include <string>
#include <cstdio>

#include <boost/ref.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION < NEW_SPIRIT_VERSION
   #define SPIRIT_NAMESPACE boost::spirit
   #include <boost/spirit/core.hpp>
#else
   #define SPIRIT_NAMESPACE boost::spirit::classic
   #include <boost/spirit/include/classic_core.hpp>
#endif

//  GLOBAL VARIABLES

// Used by grammar to determine if the parsing file is little endian
extern bool global_little_endian;

// Last int32 read. 
extern unsigned int global_parsed_int32;

// Last int64 read.
extern uint64_t global_parsed_int64;

// Last string read.
extern std::string global_parsed_string;

// Size of int32 values in the GCNO and GCDA files.
static const unsigned int int32_size = 4;

// Size of int64 values in the GCNO and GCDA files. 
static const unsigned int int64_size = 8;

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Performs parsing over the specified input using the specified grammar  
/// as the parsing rule.
///
/// @remarks
/// This function was created to parse with a type ParserT while not creating
/// a parse tree.
///
/// @param first_ The begginning of the input.
/// @param last The end of the input. 
/// @param p The parser (grammar) used to parse the input.
///
/// @return parse_info
///////////////////////////////////////////////////////////////////////////////
template <typename IteratorT, typename ParserT>
inline SPIRIT_NAMESPACE::parse_info<IteratorT>
raw_parse(
    IteratorT const& first_
  , IteratorT const& last
  , SPIRIT_NAMESPACE::parser<ParserT> const& p)
{
    // Create a scanner that will iterate from first to last byte in the 
    // input buffer.
    IteratorT first = first_;
    SPIRIT_NAMESPACE::scanner<IteratorT, SPIRIT_NAMESPACE::scanner_policies<> >
        scan(first, last);

    // Call spirit parse method on the parser p.
    SPIRIT_NAMESPACE::match<SPIRIT_NAMESPACE::nil_t> hit = p.derived().parse(scan);

    // Create and return spirit parser_info: contains the result of parsing.
    return SPIRIT_NAMESPACE::parse_info<IteratorT>(
        first, hit, hit && (first == last), hit.length());
}

///////////////////////////////////////////////////////////////////////////////
/// @brief
/// Reverses a char buffer of size 4
///
/// @param buf Pointer to buffer to be reversed
///////////////////////////////////////////////////////////////////////////////
inline void reverse_buffer(char * buf)
{
    char * i, * j;
    char temp;

    for (i = buf, j = buf + 3; i < j; i++, j--)
    {
        temp = *i;
        *i = *j;
        *j = temp;
    }
}

//  PRIMITIVE DEFINITIONS

/// @brief
/// Parser used to extract an int32 from the input
template<typename DerivedT>
struct int32_parser : public SPIRIT_NAMESPACE::parser<DerivedT>
{
   typedef DerivedT self_t;

   /// @brief
   /// Result value type. Needed for val = arg1 assignments in spirit grammar
   template <typename ScannerT>
   struct result
   {
      typedef typename SPIRIT_NAMESPACE::match_result<ScannerT,   
             typename ScannerT::value_t>::type type;
   };

   /// @brief
   /// Reads 4 bytes from the input and returns the unsigned int value.    
   ///
   /// @param scan The scanner used for parsing the input.
   ///
   /// @return Result value of scan to be used by spirit framework.
   template <typename ScannerT>
   typename SPIRIT_NAMESPACE::parser_result<self_t, ScannerT>::type
   parse(ScannerT const & scan) const
   {
      typedef typename ScannerT::iterator_t iterator_t;

      iterator_t save;
      char buf[int32_size];
      unsigned int i;

      // Save location of first scan
      if (!scan.at_end())
      {
         save = scan.first;
      }

      // Read all bytes of int32 
      for (i = 0; i < int32_size && !scan.at_end(); i++, scan.first++)
      {
         buf[i] = *scan;
      }
            
      if (i == int32_size)
      {
         // Flip big endian
         if (!global_little_endian)
         {
             reverse_buffer(buf);
         } 

         // Convert 4 bytes into unsigned int
         i = *((unsigned int *) buf);
         global_parsed_int32 = i;      
   
         if (this->derived().test(i))
         {
               return scan.create_match(int32_size, 0, save, scan.first);  
         }
      }
   
      // Failure to read int32
      return scan.no_match();
   
   } // end of parse(...)

}; // end of struct int32_parser

/// @brief
/// Primitive used to match raw byte data as an unsigned int32
template <typename IntT = unsigned int>
struct int32lit : public int32_parser< int32lit<IntT> >
{
   /// @brief
   /// Intializes the internal int32 value
   ///
   /// @param int_ Intial value of internal int32.
   int32lit(IntT value)  : int32(value) 
   {
      // Do nothing
   }

   /// @brief
   /// Tests if argument matches value desired value.  
   ///
   /// @param value Value to compare against. 
   ///
   /// @return If matches. 
   bool test(IntT value) const
   {
      return value == int32;
   }

    // Internal int32 value for matching a specific value
   IntT int32;
}; 

/// @brief
/// Primitive used to match raw byte data as an unsigned int32
template <typename IntT = unsigned int>
struct anyint32 : public int32_parser< anyint32<IntT> >
{
   /// @brief
   /// Matches any int.  
   ///
   /// @param value Ingorned 
   ///
   /// @return bool. 
   bool test(IntT value) const
   {
      return true;
   }
};

/// @brief
/// Primitive used to match raw byte data as an unsigned int32
template <typename IntT = unsigned int>
struct nonzero32 : public int32_parser< nonzero32<IntT> >
{
   /// @brief
   /// Matches any nonzero int.  
   ///
   /// @param value Value to compare against. 
   ///
   /// @return not 0 (true), 0 (false). 
   bool test(IntT value) const
   {
      return (value != 0);
   }
};


// Primitive TOKEN32
template<typename IntT> inline int32lit<IntT> TOKEN32(IntT int32)
{
   return int32lit<IntT>(int32);
} 

// Primitive INT32
anyint32<unsigned int> const INT32 = anyint32<unsigned int>();

// Primitive NONZERO
nonzero32<unsigned int> const NONZERO = nonzero32<unsigned int>();

/// @brief
/// Parser used to extract an string from the input
///
/// @remarks
/// String format: size (int32), null terminated string, padding  
template<typename StrT>
struct string_parser : public SPIRIT_NAMESPACE::parser<string_parser<StrT> >
{
   typedef string_parser<StrT> self_t;

   ///  @brief
   ///  Reads an arbitrary length string and returns the value 
   ///
   ///  @param scan The scanner used for parsing the input.
   ///
   ///  @return parser_result 
   template <typename ScannerT>
   typename SPIRIT_NAMESPACE::parser_result<self_t, ScannerT>::type
   parse(ScannerT const & scan) const
   {
      typedef typename ScannerT::iterator_t iterator_t;
      typedef typename 
         SPIRIT_NAMESPACE::parser_result<self_t, ScannerT>::type result_t;

      iterator_t save;
      std::string str;
      char buf[int32_size];
      unsigned int i, length;

      // Save location of first scan
      if (!scan.at_end())
      {
         save = scan.first;
      }

      // Read int32 length
      for (i = 0; i < int32_size && !scan.at_end(); i++, scan.first++)
      {
         buf[i] = *scan;
      }

      // Fail all bytes were not read
      if (i != int32_size)
      {
         return scan.no_match();
      }

      // Flip big endian
      if (!global_little_endian)
      {
         reverse_buffer(buf);
      } 

      // Convert bytes into length (unsigned int)
      length = *((unsigned int *) buf) * int32_size;

      // Don't read match empty string
      if (length == 0)
      {
         return scan.no_match();
      }

      // Read String
      for (i = 0; !scan.at_end() && *scan != 0; i++, scan.first++)
      {
         str.push_back(*scan);   
      }

      // Consume padding
      while (i < length && !scan.at_end())
      {
         scan.first++;
         i++;  
      }

      // If entire length of string was read, then no failure
      if (i == length)
      {
         global_parsed_string = str;
         return scan.create_match(length, str, save, scan.first);    
      }
   
      // Failure when whole string was not read
      return scan.no_match();
      
   } // end of parse(...)

}; // end of struct string_parser  

/// @brief
/// Primitive Indentifier used for primitive string_parser
string_parser<std::string> const STRING = 
    string_parser<std::string>();

/// @brief
/// Parser used to extract an int64 from the input
template<typename Int64T>
struct int64_parser : public SPIRIT_NAMESPACE::parser<int64_parser<Int64T> >
{
   typedef int64_parser<Int64T> self_t;

   ///  @brief
   ///  Reads an int64 from parsing input.
   ///  
   ///  @param scan The scanner used for parsing the input.
   ///
   ///  @return parser_result 
   template <typename ScannerT>
   typename SPIRIT_NAMESPACE::parser_result<self_t, ScannerT>::type
   parse(ScannerT const & scan) const
   {
      typedef typename ScannerT::iterator_t iterator_t;
      typedef typename 
            SPIRIT_NAMESPACE::parser_result<self_t, ScannerT>::type result_t;

      iterator_t save;
      std::string str;
      char buf[int64_size];
        unsigned int i;

      // Save location of first scan
      if (!scan.at_end())
      {
         save = scan.first;
      }

      // Read bytes of int64 
      for (i = 0; i < int64_size && !scan.at_end(); i++, scan.first++)
      {
          buf[i] = *scan;
      }

      // Fail not all bytes were not read
      if (i != int64_size)
      {
         return scan.no_match();
      }

      unsigned int low = 0;
      unsigned int high = 0;
      uint64_t value = 0;
      char * first = buf + 0;
      char * last = buf + int64_size;

      if (global_little_endian)
      {
          low = *((unsigned int *) first);
          high = *((unsigned int *) (first + int32_size));
      
          // 8 bits in 1 byte. 
          value |= low;
          value |= (uint64_t) high << int32_size * 8;
      }
      else // Big endian
      {
          char buf[int32_size];

          // Get low int32
          for (unsigned int i = 0; i < int32_size; i++)
          {
              buf[int32_size - 1 - i] = *(first + i);
          }
          low = *((unsigned int *) buf);

          // Get high int32
          for (unsigned int i = 0; i < int32_size; i++)
          {
              buf[int32_size - 1 - i] = *(first + int32_size + i);
          }
          high = *((unsigned int *) buf);

          // 8 bits in 1 byte. 
          value |= low;
          value |= (uint64_t) high << int32_size * 8;
      }

      global_parsed_int64 = value;
      return scan.create_match(int64_size, 0, save, scan.first);   
   
   } // end of parse(...)

}; // end of struct gcov_int64_parser  

/// @brief
/// Primitive Idenitifier for parsing an int64.           
int64_parser<unsigned int> const INT64 = 
    int64_parser<unsigned int>();

#endif

