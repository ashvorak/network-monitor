#include "network-monitor/TransportNetwork.h"

using NetworkMonitor::Id;
using NetworkMonitor::Line;
using NetworkMonitor::PassengerEvent;
using NetworkMonitor::Route;
using NetworkMonitor::Station;
using NetworkMonitor::TransportNetwork;

bool Station::operator==(const Station& other) const
{
	return	id == other.id 
			&& name == other.name;
}

bool Route::operator==(const Route& other) const
{
	return	id == other.id 
			&& name == other.name
			&& lineId == other.lineId
			&& startStationId == other.startStationId
			&& endStationId == other.endStationId
			&& stops == other.stops;
}

bool Line::operator==(const Line& other) const
{
	return	id == other.id 
			&& name == other.name
			&& routes == other.routes;
}

TransportNetwork::TransportNetwork() = default;

TransportNetwork::~TransportNetwork() = default;

TransportNetwork::TransportNetwork(
	const TransportNetwork& copied
) = default;

TransportNetwork::TransportNetwork(
	TransportNetwork&& moved
) = default;

TransportNetwork& TransportNetwork::operator=(
	const TransportNetwork& copied
) = default;

TransportNetwork& TransportNetwork::operator=(
	TransportNetwork&& moved
) = default;

bool TransportNetwork::FromJson(
	nlohmann::json&& src
)
{
	for (auto&& stationJson : src.at("stations")) {
		Station station {	
			std::move(stationJson.at("station_id").get<std::string>()), 
			std::move(stationJson.at("name").get<std::string>())
		};

		if (!AddStation(station)) {
			throw std::runtime_error("Can't add station : " + station.id);
		}
	}

	for (auto&& lineJson : src.at("lines")) {

		std::vector<Route> routes {};
		auto routesJson = lineJson.at("routes");

		routes.reserve(routesJson.size());
		for (auto&& routeJson : routesJson) {
			Route route {
				std::move(routeJson.at("route_id").get<std::string>()),
				std::move(routeJson.at("direction").get<std::string>()),
				std::move(routeJson.at("line_id").get<std::string>()),
				std::move(routeJson.at("start_station_id").get<std::string>()),
				std::move(routeJson.at("end_station_id").get<std::string>()),
				std::move(routeJson.at("route_stops").get<std::vector<Id>>())
			};

			routes.emplace_back(std::move(route));
		}

		Line line {
			std::move(lineJson.at("line_id").get<std::string>()), 
			std::move(lineJson.at("name").get<std::string>()),
			std::move(routes)
		};

		if (!AddLine(line)) {
			throw std::runtime_error("Can't add line : " + line.id);
		}
	}

	for (auto&& travelTimeJson : src.at("travel_times")) {

		if (!SetTravelTime(
				travelTimeJson.at("start_station_id").get<std::string>(),
				travelTimeJson.at("end_station_id").get<std::string>(),
				travelTimeJson.at("travel_time").get<uint32_t>())
			) {
			return false;
		}
	}

	return true;
}

bool TransportNetwork::AddStation(const Station& station)
{
	if (GetStation(station.id)) {
		return false;
	}

	auto node { 
		std::make_shared<GraphNode>(
			GraphNode{station.id, station.name, 0, {}}
		)};
	m_stations.emplace(station.id, std::move(node));

	return true;
}

bool TransportNetwork::AddLine(const Line& line)
{
	if (GetLine(line.id)) {
		return false;
	}

	auto lineInternal { 
		std::make_shared<LineInternal>(
			LineInternal{line.id, line.name, {}}
	)};

	for (auto& route: line.routes) {
		if (AddRouteToLine(lineInternal, route) == false)
			return false;
	}

	m_lines.emplace(line.id, std::move(lineInternal));

	return true;
}

bool TransportNetwork::RecordPassengerEvent(
    const PassengerEvent& event
)
{
	auto station {GetStation(event.stationId)};
	if (station == nullptr) {
		return false;
	}

	switch (event.type) {
		case PassengerEvent::Type::In:
			++station->passengerCount;
			break;	
		case PassengerEvent::Type::Out:
			--station->passengerCount;
			break;
		default:
			return false;
	}

	return true;
}

int64_t TransportNetwork::GetPassengerCount(
	const Id& stationId
) const
{
	const auto station {GetStation(stationId)};
	if (station == nullptr) {
		throw std::runtime_error("Can't find needed station");
	}

	return station->passengerCount;
}

std::vector<Id> TransportNetwork::GetRoutesServingStation(
	const Id& stationId
) const
{
	std::vector<Id> servingRoutes {};

	const auto station {GetStation(stationId)};
	if (station == nullptr) {
		return servingRoutes;
	}

	const auto& edges {station->edges};
	for (const auto& edge: edges) {
		servingRoutes.push_back(edge.route->id);
	}

	// The previous loop misses a corner case: The end station of a route does
	// not have any edge containing that route, because we only track the routes
	// that *leave from*, not *arrive to* a certain station.
	// We need to loop over all line routes to check if our station is the end
	// stop of any route.
	// FIXME: In the worst case, we are iterating over all routes for all
	//        lines in the network. Need to optimize this.
	for (const auto& [_, line]: m_lines) {
		for (const auto& [_, route]: line->routes) {
			const auto& endStop {route->stops[route->stops.size() - 1]};
			if (station == endStop) {
				servingRoutes.push_back(route->id);
			}
		}
	}

	return servingRoutes;
}

bool TransportNetwork::SetTravelTime(
	const Id& stationIdA,
	const Id& stationIdB,
	const uint32_t travelTime
)
{
	const auto stationA {GetStation(stationIdA)};
	const auto stationB {GetStation(stationIdB)};
	if (stationA == nullptr || stationB == nullptr) {
		return false;
	}

	bool foundEdge {false};
	auto setTravelTime = [&travelTime, &foundEdge](auto stationFrom, auto stationTo) {
		for (auto& edge : stationFrom->edges) {
			if (edge.next == stationTo) {
				edge.travelTime = travelTime;
				foundEdge = true;
			}
		}
	};
	setTravelTime(stationA, stationB);
	setTravelTime(stationB, stationA);

	return foundEdge;
}

uint32_t TransportNetwork::GetTravelTime(
	const Id& stationIdA,
	const Id& stationIdB
) const
{
	const auto stationA {GetStation(stationIdA)};
	const auto stationB {GetStation(stationIdB)};
	if (stationA == nullptr || stationB == nullptr) {
		return false;
	}

	for (const auto& edge: stationA->edges) {
		if (edge.next == stationB) {
			return edge.travelTime;
		}
	}

	for (const auto& edge: stationB->edges) {
		if (edge.next == stationA) {
			return edge.travelTime;
		}
	}

	return 0;
}

uint32_t  TransportNetwork::GetTravelTime(
	const Id& lineId,
	const Id& routeId,
	const Id& stationIdA,
	const Id& stationIdB
) const
{
	const auto stationA {GetStation(stationIdA)};
	const auto stationB {GetStation(stationIdB)};
	if (stationA == nullptr || stationB == nullptr) {
		return false;
	}

	auto line {GetLine(lineId)};
	if (line == nullptr) {
		return 0;
	}

	auto route {GetRouteFromLine(line, routeId)};
	if (route == nullptr) {
		return 0;
	}

	bool foundA {false};
	uint32_t travelTime {0};
	for (const auto& station : route->stops) {
		if (station == stationA) {
			foundA = true;
		}

		if (station == stationB) {
			return travelTime;
		}

		if (foundA) {
			auto edgeIt {
				std::find_if(
					station->edges.begin(), 
					station->edges.end(), 
					[&route](const GraphEdge& edge) {
					return edge.route == route;
				})
			};
			if (edgeIt == station->edges.end()) {
				return 0;
			}

			travelTime += edgeIt->travelTime;
		}
	}

	return 0;
}

// Private functions

std::shared_ptr<TransportNetwork::GraphNode> TransportNetwork::GetStation(
	const Id& stationId
) const
{
	const auto stationIt {m_stations.find(stationId)};
	if (stationIt == m_stations.end()) {
		return nullptr;
	}

	return stationIt->second;
}

std::shared_ptr<TransportNetwork::LineInternal> TransportNetwork::GetLine(
	const Id& lineId
) const
{
	const auto lineIt {m_lines.find(lineId)};
	if (lineIt == m_lines.end()) {
		return nullptr;
	}

	return lineIt->second;	
}

std::shared_ptr<TransportNetwork::RouteInternal> TransportNetwork::GetRouteFromLine(
	std::shared_ptr<TransportNetwork::LineInternal> line,
	const Id& routeId
) const
{
	const auto routeIt {line->routes.find(routeId)};
	if (routeIt == line->routes.end()) {
		return nullptr;
	}

	return routeIt->second;		
}

bool TransportNetwork::AddRouteToLine(
	std::shared_ptr<TransportNetwork::LineInternal> line,
	const Route& route
)
{
	if (line->routes.find(route.id) != line->routes.end()) {
		return false;
	}

	auto routeInternal {
		std::make_shared<RouteInternal>(
			RouteInternal{
				route.id,
				route.name,
				line,
				{}
			}
		)
	};

	routeInternal->stops.reserve(route.stops.size());
	for (const auto& stationId: route.stops) {
		const auto station {m_stations.find(stationId)};
		if (station == m_stations.end()) {
			return false;
		}

		routeInternal->stops.push_back(station->second);
	}

	for (auto idx {0}; idx < routeInternal->stops.size() - 1; ++ idx) {
		const auto& thisStop {routeInternal->stops[idx]};
		const auto& nextStop {routeInternal->stops[idx + 1]};

		GraphEdge edge {
			routeInternal,
			nextStop,
			0
		};

		thisStop->edges.emplace_back(std::move(edge));
	}
	line->routes.emplace(route.id, std::move(routeInternal));

	return true;
}
