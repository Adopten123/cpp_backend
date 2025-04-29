#include "model.h"

#include <stdexcept>

namespace model {
    using namespace std::literals;

    std::vector<const Dog*> GameSession::GetDogs() const {
    	std::vector<const Dog*> result;
    	result.reserve(dogs_.size()-1);
    	for (const auto& dog : dogs_) {
        	result.emplace_back(&dog);
    	}
    	return result;
	}

	Dog* GameSession::AddDog(Dog&& dog) {
    	dogs_.emplace_back(std::move(dog));
    	return &dogs_.back();
	}

    void Map::AddOffice(Office office) {
        if (warehouse_id_to_index_.contains(office.GetId())) {
            throw std::invalid_argument("Duplicate warehouse");
        }

        const size_t index = offices_.size();
        Office& o = offices_.emplace_back(std::move(office));
        try {
            warehouse_id_to_index_.emplace(o.GetId(), index);
        } catch (...) {
            // Удаляем офис из вектора, если не удалось вставить в unordered_map
            offices_.pop_back();
            throw;
        }
    }

    void Game::AddMap(Map map) {
        const size_t index = maps_.size();
        if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
            throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
        } else {
            try {
                maps_.emplace_back(std::move(map));
            } catch (...) {
                map_id_to_index_.erase(it);
                throw;
            }
        }
    }

}  // namespace model