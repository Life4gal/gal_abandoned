#pragma once

#ifndef GAL_LANG_AST_LEXER_HPP
#define GAL_LANG_AST_LEXER_HPP

#include <variant>

#include <utils/point.hpp>
#include <utils/allocator.hpp>
#include <utils/assert.hpp>
#include <utils/enum_utils.hpp>
#include <utils/confusable.hpp>
#include <utils/hash_container.hpp>

#include <ast/ast.hpp>

namespace gal
{
	class lexeme_point
	{
	public:
		enum class token_type
		{
			eof = 0,
			char_sentinel_begin = 0,

			// 1 ~ 255 => character

			char_sentinel_end = 256,

			// ==
			equal,
			// !=
			not_equal,
			// <
			less_than,
			// <=
			less_equal,
			// >
			greater_than,
			// >=
			greater_equal,

			// +=
			plug_assign,
			// -=
			minus_assign,
			// *=
			multiply_assign,
			// /=
			divide_assign,
			// %=
			modulus_assign,
			// **=
			pow_assign,

			raw_string,
			quoted_string,
			number,
			name,

			comment,
			block_comment,

			// ::
			double_colon,
			// ->
			right_arrow,

			broken_string,
			broken_comment,
			broken_unicode,
			error,

			keyword_sentinel_begin,

			keyword_and,
			keyword_break,
			keyword_do,
			keyword_else,
			// ReSharper disable once IdentifierTypo
			keyword_elif,
			keyword_end,
			keyword_false,
			keyword_for,
			keyword_function,
			keyword_if,
			keyword_in,
			keyword_local,
			keyword_null,
			keyword_not,
			keyword_or,
			keyword_repeat,
			keyword_return,
			keyword_then,
			keyword_true,
			keyword_until,
			keyword_while,

			keyword_sentinel_end,
		};

		constexpr static std::string_view keywords[] =
		{
				{"and"},
				{"break"},
				{"do"},
				{"else"},
				// ReSharper disable once StringLiteralTypo
				{"elif"},
				{"end"},
				{"false"},
				{"for"},
				{"function"},
				{"if"},
				{"in"},
				{"local"},
				{"null"},
				{"not"},
				{"or"},
				{"repeat"},
				{"return"},
				{"then"},
				{"true"},
				{"until"},
				{"while"}
		};

		using token_underlying_type = std::underlying_type_t<token_type>;
		using length_type = std::size_t;
		using data_type = std::variant<const char*, std::uint32_t>;

		static_assert(std::size(keywords) == static_cast<token_underlying_type>(token_type::keyword_sentinel_end) - static_cast<token_underlying_type>(token_type::keyword_sentinel_begin) + 1 - 2);

	private:
		token_type type_;
		location loc_;

		/*
		 * data -> string/number/comment
		 * name -> name
		 * codepoint -> broken unicode
		 */
		data_type data_;
		length_type length_;

		lexeme_point(const token_type type, const location loc)
			: type_{type},
			  loc_{loc},
			  data_{nullptr},
			  length_{0} { }

		lexeme_point(const token_underlying_type type, const location loc)
			: lexeme_point(static_cast<token_type>(type), loc) {}

		lexeme_point(const token_type type, const location loc, const char* data, length_type length)
			: type_{type},
			  loc_{loc},
			  data_{data},
			  length_{length} { gal_assert(is_any_enum_of(type, token_type::raw_string, token_type::quoted_string, token_type::number, token_type::comment, token_type::block_comment), "Mismatch type! Type should be string/number/comment."); }

		lexeme_point(const token_type type, const location loc, const char* name)
			: type_{type},
			  loc_{loc},
			  data_{name},
			  length_{0} { gal_assert(is_any_enum_of(type, token_type::name) || is_enum_between_of<false, false>(type_, token_type::keyword_sentinel_begin, token_type::keyword_sentinel_end), "Mismatch type! Type should be name/keyword"); }

		[[nodiscard]] constexpr bool is_comment() const noexcept { return is_any_enum_of(type_, token_type::comment, token_type::block_comment); }

		[[nodiscard]] std::string to_string() const noexcept
		{
			switch (type_)
			{
				case token_type::eof: { return "<eof>"; }
				case token_type::equal: { return "'=='"; }
				case token_type::not_equal: { return "'!='"; }
				case token_type::less_than: { return "'<'"; }
				case token_type::less_equal: { return "'<='"; }
				case token_type::greater_than: { return "'>'"; }
				case token_type::greater_equal: { return "'>='"; }
				case token_type::plug_assign: { return "'+='"; }
				case token_type::minus_assign: { return "'-='"; }
				case token_type::multiply_assign: { return "'*='"; }
				case token_type::divide_assign: { return "'/='"; }
				case token_type::modulus_assign: { return "'%='"; }
				case token_type::pow_assign: { return "'**='"; }
				case token_type::raw_string:
				case token_type::quoted_string:
				case token_type::number:
				case token_type::name:
				{
					gal_assert(std::holds_alternative<const char*>(data_), "We should be holding a string, but in fact we don't.");
					if (const auto* data = std::get<const char*>(data_); data)
					{
						[[likely]]
								return std_format::format("{:<{}}", length_, data);
					}

					[[unlikely]]
					if (type_ == token_type::raw_string || type_ == token_type::quoted_string) { return "<string>"; }
					if (type_ == token_type::number) { return "<number>"; }
					return "<identifier>";
				}
				case token_type::comment:
				case token_type::block_comment: { return "<comment>"; }
				case token_type::double_colon: { return "'::'"; }
				case token_type::right_arrow: { return "'->'"; }
				case token_type::broken_string: { return "<malformed string>"; }
				case token_type::broken_comment: { return "<unfinished comment>"; }
				case token_type::broken_unicode:
				{
					gal_assert(std::holds_alternative<std::uint32_t>(data_), "We should be holding a codepoint, but in fact we don't.");
					const auto codepoint = std::get<std::uint32_t>(data_);
					if (const auto* confusable = find_confusable(codepoint); confusable) { return std_format::format("Unicode character U+{:#0x} (did you mean '{}'?)", codepoint, confusable); }
					return std_format::format("Unicode character U+{:#0x}", codepoint);
				}
				default:
				{
					if (is_enum_between_of<false, false>(type_, token_type::char_sentinel_begin, token_type::char_sentinel_end)) { return std_format::format("{}", static_cast<char>(type_)); }
					if (is_enum_between_of<false, false>(type_, token_type::keyword_sentinel_begin, token_type::keyword_sentinel_end)) { return std_format::format("{}", keywords[static_cast<token_underlying_type>(type_) - static_cast<token_underlying_type>(token_type::keyword_sentinel_begin) - 1]); }
					return "<unknown>";
				}
			}
		}
	};

	class ast_name_table
	{
	private:
		struct table_entry
		{
			ast_name name;
			lexeme_point::token_type type;

			[[nodiscard]] constexpr bool operator==(const table_entry& rhs) const noexcept { return name == rhs.name; }

			[[nodiscard]] constexpr bool empty() const noexcept { return type == lexeme_point::token_type::eof; }

			struct table_entry_hasher
			{
				[[nodiscard]] std::size_t operator()(const table_entry& entry) const noexcept
				{
					// FNV-1a hash. See: http://www.isthe.com/chongo/tech/comp/fnv/
					constexpr std::uint64_t hash_init{14695981039346656037ull};
					constexpr std::uint64_t hash_prime{1099511628211ull};

					auto hash = hash_init;
					for (const auto c: entry.name)
					{
						hash ^= c;
						hash *= hash_prime;
					}
					return hash;
				}
			};
		};

		hash_set<table_entry, table_entry::table_entry_hasher> data_;
		managed_allocator& allocator_;
	public:
		explicit ast_name_table(managed_allocator& allocator);
	};
}

#endif // GAL_LANG_AST_LEXER_HPP