#include "api_handler.h"

namespace http_handler {

ExtensionToConetntTypeMapper Sender::mapper_;


APIRequestHandler::APIRequestHandler(app::Application& app, net::io_context& ioc, bool no_auto_tick)
    : app_{ app },
    strand_(net::make_strand(ioc)),
    auto_tick_(!no_auto_tick){
}

json::array APIRequestHandler::ProcessMapsRequestBody() const {
    auto maps_body = json::array();
    for (const auto& map : app_.GetMaps()) {
        json::object body;
        body[std::string(model::ModelLiterals::ID)] = *map.GetId();
        body[std::string(model::ModelLiterals::NAME)] = map.GetName();
        maps_body.emplace_back(std::move(body));
    }
    return maps_body;
}

ExtensionToConetntTypeMapper::ExtensionToConetntTypeMapper() {
    map_[FileExtensions::HTML] = MimeType::TEXT_HTML;
    map_[FileExtensions::HTM] = MimeType::TEXT_HTML;
    map_[FileExtensions::JSON] = MimeType::APP_JSON;
    map_[FileExtensions::CSS] = MimeType::TEXT_CSS;
    map_[FileExtensions::TXT] = MimeType::TEXT_PLAIN;
    map_[FileExtensions::JS] = MimeType::TEXT_JAVASCRIPT;
    map_[FileExtensions::XML] = MimeType::APP_XML;
    map_[FileExtensions::PNG] = MimeType::PNG;
    map_[FileExtensions::JPEG] = MimeType::JPEG;
    map_[FileExtensions::JPG] = MimeType::JPEG;
    map_[FileExtensions::JPE] = MimeType::JPEG;
    map_[FileExtensions::GIF] = MimeType::GIF;
    map_[FileExtensions::BMP] = MimeType::BMP;
    map_[FileExtensions::ICO] = MimeType::ICO;
    map_[FileExtensions::TIFF] = MimeType::TIFF;
    map_[FileExtensions::TIF] = MimeType::TIFF;
    map_[FileExtensions::SVG] = MimeType::SVG;
    map_[FileExtensions::SVGZ] = MimeType::SVG;
    map_[FileExtensions::MP3] = MimeType::MP3;
}

std::string_view ExtensionToConetntTypeMapper::operator()(std::string_view extension) {
    if (map_.contains(extension)) {
        return map_.at(extension);
    }
    return MimeType::UNKNOWN;
}


bool APIRequestHandler::ParseBearer(const std::string_view auth_header, std::string& token_to_write) const {
    if (!auth_header.starts_with("Bearer ")) {
        return false;
    }
    std::string_view str = auth_header.substr(7);
    if (str.size() != 32) {
        return false;
    }
    token_to_write = str;
    return true;
}

}