#include <karl/image_citations.hpp>

#include <fstream>
#include <iostream>
#include <ImageMagick/Magick++.h>
#include <boost/lexical_cast.hpp>

namespace supermarx
{

image_citations::image_citations(std::string const& _path)
: path(_path)
{
	static bool initialized = false;

	if(!initialized)
	{
		Magick::InitializeMagick(NULL);
		initialized = true;
	}
}

std::pair<size_t, size_t> image_citations::get_size(const raw &_r) const
{
	Magick::Blob img_blob(_r.data(), _r.length());

	Magick::Image img;
	img.read(img_blob);

	Magick::Geometry geo(img.size());
	return std::make_pair(geo.width(), geo.height());
}

void image_citations::commit(id_t id, const raw &_r, std::pair<size_t, size_t> const& new_geo)
{
	Magick::Blob img_blob(_r.data(), _r.length());

	Magick::Image img;
	img.read(img_blob);

	img.write(path+boost::lexical_cast<std::string>(id)+"_orig.png");

	img.resize(boost::lexical_cast<std::string>(new_geo.first) + "x" + boost::lexical_cast<std::string>(new_geo.second));

	img.write(path+boost::lexical_cast<std::string>(id)+".png");
}

}
