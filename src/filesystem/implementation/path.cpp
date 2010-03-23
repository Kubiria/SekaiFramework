/*========================================================
* path.cpp
* @author Sergey Mikhtonyuk
* @date 20 Mar 2010
*
* Copyrights (c) Sergey Mikhtonyuk 2007-2010.
* Terms of use, copying, distribution, and modification
* are covered in accompanying LICENSE file
=========================================================*/
#include "path.h"
#include "directory_iterator.h"
#include "module/exception.h"
#include <string>

#if defined OS_WINDOWS
#	include <windows.h>
#endif

namespace filesystem
{
	//////////////////////////////////////////////////////////////////////////

	static bool is_sep(char c)
	{
		const char* seps = path::separators();
		while(*seps)
		{
			if(c == *seps) return true;
			++seps;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// path impl
	//////////////////////////////////////////////////////////////////////////

	struct path::path_impl
	{
		//////////////////////////////////////////////////////////////////////////

		path_impl() 
		{ }

		//////////////////////////////////////////////////////////////////////////

		path_impl(const char* p)
		{
			for(const char* pp = p; *pp; ++pp)
				if(is_invalid_path_symbol(*pp))
					throw Module::InvalidArgumentException("Invalid symbols in the path");

			size_t l = strlen(p);
			while(--l > 0 && is_sep(p[l]))
				;
			if(l > 0)
				str.assign(p, l + 1);
		}

		//////////////////////////////////////////////////////////////////////////

		path_impl(const path_impl& other)
			: str(other.str)
		{ }

		//////////////////////////////////////////////////////////////////////////

		path_impl& operator=(const path_impl& rhs)
		{
			if(this == &rhs)
				return *this;

			str = rhs.str;
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////

		path_impl& operator+=(const path_impl& rhs)
		{
			if(!rhs.str.empty())
			{
				size_t size = str.size();

				const char* as = rhs.str.c_str();
				
				if(size) // skip as a trick for unix root paths
					while(as && is_sep(*as))
						++as;

				if(size && !is_sep(str[size - 1]))
					str += *path::separators();

				str.append(as);
			}
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////

		bool operator==(const path_impl& rhs) const
		{
			return str == rhs.str;
		}

		//////////////////////////////////////////////////////////////////////////

		bool operator!=(const path_impl& rhs) const
		{
			return ! (*this == rhs );
		}

		//////////////////////////////////////////////////////////////////////////

		std::string str;
	};

	//////////////////////////////////////////////////////////////////////////




	//////////////////////////////////////////////////////////////////////////
	// path
	//////////////////////////////////////////////////////////////////////////

	path::path() : m_impl(new path_impl()) { }

	//////////////////////////////////////////////////////////////////////////

	path::path(const char* p) : m_impl(new path_impl(p)) { }

	//////////////////////////////////////////////////////////////////////////

	path::path(const path& other) : m_impl(new path_impl(*other.m_impl)) { }

	//////////////////////////////////////////////////////////////////////////

	path::~path() { delete m_impl; }

	//////////////////////////////////////////////////////////////////////////

	path& path::operator=(const filesystem::path &rhs)
	{
		if(this == &rhs)
			return *this;

		*m_impl = *rhs.m_impl;
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////

	const char* path::c_str() const
	{
		return m_impl->str.c_str();
	}

	//////////////////////////////////////////////////////////////////////////

	path::iterator path::begin() const
	{
		return iterator(*this, 0);
	}

	//////////////////////////////////////////////////////////////////////////

	path::iterator path::end() const
	{
		return iterator(*this, m_impl->str.length());
	}

	//////////////////////////////////////////////////////////////////////////

	path& path::operator +=(const path &rhs)
	{
		*m_impl += *rhs.m_impl;
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////

	path path::operator +(const path &rhs) const
	{
		path ret(*this);
		return ret += rhs;
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::operator ==(const path& rhs) const
	{
		return *m_impl == *rhs.m_impl;
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::operator !=(const path& rhs) const
	{
		return !( *this == rhs );
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::exists() const
	{
		return !(get_attributes() & fa_not_found);
	}

	//////////////////////////////////////////////////////////////////////////
	
	//bool is_absolute() const;

	//////////////////////////////////////////////////////////////////////////

	bool path::is_file() const
	{
		return (get_attributes() & fa_file) ? true : false;
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::is_directory() const
	{
		return get_attributes() & fa_directory;
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::is_link() const
	{
		throw Module::NotImplementedException("Not implemented");
	}

	//////////////////////////////////////////////////////////////////////////

	int path::get_attributes() const
	{
		DWORD attrs = ::GetFileAttributesA(m_impl->str.c_str());
		if(attrs == 0xffffffff)
		{
			DWORD ec = ::GetLastError();
			if(ec == ERROR_FILE_NOT_FOUND
				|| ec == ERROR_PATH_NOT_FOUND
				|| ec == ERROR_INVALID_NAME
				|| ec == ERROR_INVALID_DRIVE
				|| ec == ERROR_INVALID_PARAMETER
				|| ec == ERROR_BAD_PATHNAME
				|| ec == ERROR_BAD_NETPATH)
			{
				return fa_not_found;
			}
			return fa_unknown;
		}
		int ret = (attrs & FILE_ATTRIBUTE_DIRECTORY) ? fa_directory : fa_file;
		if(attrs & FILE_ATTRIBUTE_HIDDEN) ret |= fa_hidden;
		if(attrs & FILE_ATTRIBUTE_READONLY) ret |= fa_readonly;
		return ret;
	}

	//////////////////////////////////////////////////////////////////////////

	directory_iterator path::list_dir() const
	{
		return directory_iterator(*this);
	}

	//////////////////////////////////////////////////////////////////////////

	const char* path::separators() 
	{ 
		return "\\/";
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::is_invalid_file_name_symbol(char c)
	{
		unsigned char code = (unsigned char)c;
		return code < 32 || c == '"' || c == '<' || c == '>' || c == '|' 
			 || c == ':' || c == '*' || c == '?' || c == '\\' || c == '/' ;
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::is_invalid_path_symbol(char c)
	{
		unsigned char code = (unsigned char)c;
		return code < 32 || c == '"' || c == '<' || c == '>' || c == '|';
	}

	//////////////////////////////////////////////////////////////////////////

	path path::current_dir()
	{
		DWORD len = ::GetCurrentDirectoryA(0, 0);
		path cd;
		cd.m_impl->str.resize(len);
		::GetCurrentDirectoryA(len, &cd.m_impl->str[0]);
		return cd;
	}
	
	//////////////////////////////////////////////////////////////////////////

	std::ostream& operator<<(std::ostream& os, const path& p)
	{
		os << p.c_str();
		return os;
	}

	//////////////////////////////////////////////////////////////////////////

	std::istream& operator>>(std::istream& is, path& p)
	{
		std::string s;
		is >> s;
		p = path(s.c_str());
		return is;
	}

	//////////////////////////////////////////////////////////////////////////




	//////////////////////////////////////////////////////////////////////////
	// path iterator impl
	//////////////////////////////////////////////////////////////////////////

	struct path::iterator::iterator_impl
	{

		//////////////////////////////////////////////////////////////////////////

		iterator_impl(const path& parent, size_t position)
			: p(&parent)
			, pos(position)
			, pstr(&parent.m_impl->str)
		{
			pos = skip_seps(pos);
			set_element(pos);
		}

		//////////////////////////////////////////////////////////////////////////

		iterator_impl(const iterator_impl& other)
			: p(other.p)
			, pos(other.pos)
			, pstr(other.pstr)
			, element(other.element)
		{ }

		//////////////////////////////////////////////////////////////////////////

		iterator_impl& operator=(const iterator_impl& rhs)
		{
			if(this == &rhs)
				return *this;

			p = rhs.p;
			pos = rhs.pos;
			pstr = rhs.pstr;
			element = rhs.element;
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////

		iterator_impl& operator++()
		{
			assert(element.size());
			pos += element.size();
			pos = skip_seps(pos);
			set_element(pos);
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////

		iterator_impl operator++(int)
		{
			iterator_impl ret(*this);
			++*this;
			return ret;
		}

		//////////////////////////////////////////////////////////////////////////

		iterator_impl& operator--()
		{
			size_t np = rskip_seps(pos);
			np = rnext_sep(np);
			np = skip_seps(np);
			assert(np != pos);
			pos = np;
			set_element(np);
			return *this;
		}

		//////////////////////////////////////////////////////////////////////////

		iterator_impl operator--(int)
		{
			iterator_impl ret(*this);
			--*this;
			return ret;
		}

		//////////////////////////////////////////////////////////////////////////

		operator bool() const
		{
			return pos != pstr->size();
		}

		//////////////////////////////////////////////////////////////////////////

		bool operator==(const iterator_impl& rhs) const
		{
			return pos == rhs.pos;
		}

		//////////////////////////////////////////////////////////////////////////

		bool operator!=(const iterator_impl& rhs) const
		{
			return !(*this == rhs);
		}

		//////////////////////////////////////////////////////////////////////////

		void set_element(size_t start)
		{
			element.clear();
			size_t end = next_sep(start);
			if(start != end)
				element.assign(&(*pstr)[start], end - start);
		}

		//////////////////////////////////////////////////////////////////////////

		size_t skip_seps(size_t _p) const
		{
			const char* s = pstr->c_str();
			while( s[_p] && is_sep(s[_p]) )
				++_p;
			return _p;
		}

		size_t rskip_seps(size_t _p) const
		{
			const char* s = pstr->c_str();
			while (--_p && is_sep(s[_p]))
				;
			return _p;
		}

		//////////////////////////////////////////////////////////////////////////

		size_t next_sep(size_t _p) const
		{
			const char* s = pstr->c_str();
			while( s[_p] && !is_sep(s[_p]) )
				++_p;
			return _p;
		}

		size_t rnext_sep(size_t _p) const
		{
			const char* s = pstr->c_str();
			while (--_p && !is_sep(s[_p]))
				;
			return _p;
		}

		//////////////////////////////////////////////////////////////////////////

		std::string element;
		const path* p;
		std::string* pstr;
		size_t pos;
	};

	//////////////////////////////////////////////////////////////////////////





	//////////////////////////////////////////////////////////////////////////
	// path iterator
	//////////////////////////////////////////////////////////////////////////

	path::iterator::iterator(const path& parent, size_t position) 
		: m_impl(new iterator_impl(parent, position))
	{ }

	//////////////////////////////////////////////////////////////////////////

	path::iterator::iterator(const path::iterator &other)
		: m_impl(new iterator_impl(*other.m_impl))
	{ }

	//////////////////////////////////////////////////////////////////////////

	path::iterator::~iterator()
	{
		delete m_impl;
	}

	//////////////////////////////////////////////////////////////////////////

	path::iterator& path::iterator::operator=(const path::iterator& rhs)
	{
		if(this == &rhs)
			return *this;

		*m_impl = *rhs.m_impl;
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////

	path::iterator& path::iterator::operator++()
	{
		++*m_impl;
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////

	path::iterator path::iterator::operator++(int)
	{
		iterator ret(*this);
		++*this;
		return ret;
	}

	//////////////////////////////////////////////////////////////////////////

	path::iterator& path::iterator::operator--()
	{
		--*m_impl;
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////

	path::iterator path::iterator::operator--(int)
	{
		iterator ret(*this);
		--*this;
		return ret;
	}

	//////////////////////////////////////////////////////////////////////////

	path::iterator::operator bool() const
	{
		return *m_impl;
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::iterator::operator ==(const path::iterator &rhs) const
	{
		return *m_impl == *rhs.m_impl;
	}

	//////////////////////////////////////////////////////////////////////////

	bool path::iterator::operator !=(const path::iterator &rhs) const
	{
		return !(*m_impl == *rhs.m_impl);
	}

	//////////////////////////////////////////////////////////////////////////

	const char* path::iterator::element() const
	{
		return m_impl->element.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	
} // namespace