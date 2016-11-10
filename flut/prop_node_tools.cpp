#include "prop_node_tools.hpp"

#include <contrib/rapidxml-1.13/rapidxml.hpp>
#include <fstream>
#include "system/path.hpp"
#include "string_tools.hpp"

namespace flut
{
	FLUT_API prop_node load_file( const path& filename )
	{
		if ( filename.extension() == "xml" )
			return load_xml( filename );
		else return load_prop( filename );
	}

	prop_node read_rapid_xml_node( rapidxml::xml_node<>* node )
	{
		// make new prop_node
		prop_node pn = make_prop_node( node->value() );

		// add attributes
		for ( rapidxml::xml_attribute<>* attr = node->first_attribute(); attr; attr = attr->next_attribute() )
			pn.add( attr->name(), attr->value() );

		// add child nodes
		for ( rapidxml::xml_node<>* child = node->first_node(); child; child = child->next_sibling() )
		{
			if ( child->name_size() > 0 )
				pn.add_child( child->name(), read_rapid_xml_node( child ) );
		}

		return pn;
	}

	FLUT_API prop_node load_xml( const path& filename )
	{
		string file_contents = load_string( filename );
		rapidxml::xml_document<> doc;
		doc.parse< 0 >( &file_contents[ 0 ] ); // not officially supported but probably safe

		if ( doc.first_node() )
		{
			prop_node pn;
			pn.add_child( doc.first_node()->name(), read_rapid_xml_node( doc.first_node() ) );
			return pn;
		}
		else return prop_node();
	}

	bool is_valid_prop_label( const string& s )
	{
		return ( s.size() > 0 && isalpha( s[ 0 ] ) );
	}

	string get_prop_token( char_stream& str )
	{
		while ( true )
		{
			string t = str.get_token( "={};" );
			if ( t.empty() ) return t;
			if ( t[ 0 ] == ';' )
			{
				// comment: skip rest of line
				str.get_line();
				continue;
			}
			else return t;
		}
	}

	void read_prop_node( char_stream& str, prop_node& parent )
	{
		string t = get_prop_token( str );
		if ( t == "=" )
		{
			parent.set_value( get_prop_token( str ) );
		}
		else if ( t == "{" )
		{
			while ( str.good() && t != "}" )
			{
				t = get_prop_token( str );
				if ( is_valid_prop_label( t ) )
					read_prop_node( str, parent.add_child( t ) );
				else if ( t != "}" )
					flut_error( "Invalid token: " + t );
			}
		}
	}

	FLUT_API prop_node load_prop( const path& filename )
	{
		auto str = load_char_stream( filename.str() );
		prop_node root;
		string t = get_prop_token( str );
		while ( is_valid_prop_label( t ) )
		{
			read_prop_node( str, root.add_child( t ) );
			t = get_prop_token( str );
		}
		return root;
	}

	void write_prop_none( std::ostream& str, const string& label, const prop_node& pn, int level, bool readable )
	{
		string indent = readable ? string( level, '\t' ) : "";
		string newline = readable ? "\n" : " ";
		string assign = readable ? " = " : "=";

		str << indent << label;
		if ( pn.has_value() )
			str << assign << '\"' << pn.get_value() << '\"'; // #TODO only add quotes when needed
		str << newline;
		if ( pn.has_children() )
		{
			str << indent << "{" << newline; // #TODO only do newline when needed
			for ( auto& node : pn )
				write_prop_none( str, node.first, node.second, level + 1, readable );
			str << indent << "}" << newline;
		}
	}

	FLUT_API void save_prop( const prop_node& pn, const path& filename, bool readable )
	{
		std::ofstream str( filename.str() );
		for ( auto& node : pn )
		{
			write_prop_none( str, node.first, node.second, 0, readable );
		}
	}

	void merge_prop_nodes( prop_node& pn, const prop_node& other, bool overwrite )
	{
		for ( auto& o : other )
		{
			auto it = pn.find_child( o.first );
			if ( it == pn.end() )
				pn.add_child( o.first, o.second );
			else if ( overwrite )
				it->second = o.second;
		}
	}

	void resolve_include_files( prop_node &pn, const path &filename, const string& include_directive, int level )
	{
		for ( auto iter = pn.begin(); iter != pn.end(); )
		{
			if ( iter->first == include_directive )
			{
				// load included file using filename path
				path include_path = filename.parent_path() / iter->second.get< path >( "file" );
				bool merge_children = iter->second.get< bool >( "merge_children", false );
				auto included_props = load_file_with_include( include_path, include_directive, level + 1 );

				// remove the include node
				iter = pn.erase( iter );

				// merge or include, depending on options
				if ( merge_children )
				{
					merge_prop_nodes( pn, included_props, false );
					iter = pn.begin(); // reset the iterator, which has become invalid after merge
				}
				else
				{
					// insert the children at the INCLUDE spot
					iter = pn.insert_children( iter, included_props.begin(), included_props.end() );
				}
			}
			else
			{
				// search in children
				resolve_include_files( iter->second, filename, include_directive, level );
				++iter;
			}
		}
	}

	prop_node load_file_with_include( const path& filename, const string& include_directive, int level )
	{
		flut_error_if( level >= 100, "Exceeded maximum include level, check for loops in includes" );

		prop_node pn = load_file( filename );
		resolve_include_files( pn, filename, include_directive, level );

		return pn;
	}
}
