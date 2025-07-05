#include "../include/handlers/health_handler.hpp"
#include <userver/formats/json/value_builder.hpp>

namespace cardbattle {

std::string HealthCheckHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request, userver::server::request::RequestContext&) const {
    auto& response = request.GetHttpResponse();
    response.SetHeader(std::string("Access-Control-Allow-Origin"), "*");
    response.SetHeader(std::string("Access-Control-Allow-Headers"), "Content-Type, Authorization");
    response.SetHeader(std::string("Access-Control-Allow-Methods"), "POST, GET, OPTIONS");
    userver::formats::json::ValueBuilder response_builder;
    response_builder["status"] = "ok";
    response_builder["message"] = "Card Battle Server is running";
    response_builder["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
    
    return userver::formats::json::ToString(response_builder.ExtractValue());
}

} // namespace cardbattle 