#pragma once

#include <supermarx/raw.hpp>
#include <supermarx/id_t.hpp>

namespace supermarx
{

class image_citations
{
	std::string path;

public:
	image_citations(std::string const& path);

	std::pair<size_t, size_t> get_size(raw const& image) const;
	void commit(id_t id, raw const& image, std::pair<size_t, size_t> const& new_geo = {150, 150});
};

}
