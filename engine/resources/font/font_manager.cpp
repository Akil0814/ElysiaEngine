#include "../../tools/logger.h"
#include "font_manager.h"
namespace elysia::resources
{
FontManager::~FontManager()
{
	clear();
}

bool FontManager::load_font(
	const std::string& key,
	const std::filesystem::path& file_path,
	int point_size
)
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Load font failed: key is empty.");
		return false;
	}

	if (file_path.empty())
	{
		ELYSIA_LOG_WARN("resource","Load font failed: file path is empty: " << key);
		return false;
	}

	if (point_size <= 0)
	{
		ELYSIA_LOG_WARN("resource","Load font failed: point size is invalid: " << key);
		return false;
	}

	TTF_Font* font = TTF_OpenFont(file_path.string().c_str(), point_size);
	if (!font)
	{
		ELYSIA_LOG_WARN("resource","Load font failed: " << file_path
			<< " error: " << TTF_GetError());
		return false;
	}

	return store_font(key, font);
}

bool FontManager::store_font(const std::string& key, TTF_Font* font)
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Store font failed: key is empty.");
		if (font)
		{
			TTF_CloseFont(font);
		}
		return false;
	}

	if (!font)
	{
		ELYSIA_LOG_WARN("resource","Store font failed: font is null: " << key);
		return false;
	}

	FontPool::iterator iterator = _font_pool.find(key);
	if (iterator != _font_pool.end())
	{
		if (iterator->second)
		{
			TTF_CloseFont(iterator->second);
		}

		iterator->second = font;
		return true;
	}

	_font_pool.emplace(key, font);
	return true;
}

bool FontManager::has_font(std::string_view key) const noexcept
{
	return !key.empty() && _font_pool.contains(std::string(key));
}

TTF_Font* FontManager::find_font(const std::string_view& key) const
{
	if (key.empty())
	{
		ELYSIA_LOG_WARN("resource","Find font failed: key is empty.");
		return nullptr;
	}

	FontPool::const_iterator iterator = _font_pool.find(std::string(key));
	if (iterator == _font_pool.end())
	{
		ELYSIA_LOG_WARN("resource","Find font failed: resource does not exist: "
			<< key);
		return nullptr;
	}

	return iterator->second;
}

void FontManager::clear()
{
	for (FontPool::value_type& font : _font_pool)
	{
		if (font.second)
			TTF_CloseFont(font.second);
	}

	_font_pool.clear();
}

size_t FontManager::resource_count() const
{
	return _font_pool.size();
}

}
