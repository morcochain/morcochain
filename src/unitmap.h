#ifndef UNIT_MAP_H
#define UNIT_MAP_H

#define UNIT_COUNT 2

// A map class to store per-unit values.
// It uses less memory than a std::map.
template<typename T>
class CUnitMap
{
    T data[UNIT_COUNT];
    char set;

    static int GetUnitIndex(unsigned char cUnit)
    {
        switch (cUnit)
        {
            case 'S': return 0;
            case 'B': return 1;
            default: throw std::runtime_error("Invalid unit");
        }
    }

    static int GetUnit(int index)
    {
        switch (index)
        {
            case 0: return 'S';
            case 1: return 'B';
            default: throw std::runtime_error("Invalid index");
        }
    }

public:
    CUnitMap() : set(0)
    {
    }

    bool HasIndex(int index) const
    {
        return (set & (1 << index)) != 0;
    }

    bool Has(unsigned char cUnit) const
    {
        return HasIndex(GetUnitIndex(cUnit));
    }

    void Set(unsigned char cUnit)
    {
        set |= (1 << GetUnitIndex(cUnit));
    }

    typedef std::map<unsigned char, T> MapType;
    void SetFromMap(const MapType& map)
    {
        clear();
        BOOST_FOREACH(PAIRTYPE(unsigned char, T) pair, map)
        {
            Set(pair.first);
            data[GetUnitIndex(pair.first)] = pair.second;
        }
    }

    MapType GetMap() const
    {
        MapType map;
        for (int i = 0; i < UNIT_COUNT; i++)
            if (HasIndex(i))
                map[GetUnit(i)] = data[i];
        return map;
    }

    IMPLEMENT_SERIALIZE
    (
        MapType map;
        if (fWrite)
            map = GetMap();
        READWRITE(map);
        if (fRead)
            const_cast<CUnitMap*>(this)->SetFromMap(map);
    )

    T const& operator[](unsigned char cUnit) const
    {
        assert(Has(cUnit));
        return data[GetUnitIndex(cUnit)];
    }

    T& operator[](unsigned char cUnit)
    {
        Set(cUnit);
        return data[GetUnitIndex(cUnit)];
    }

    void clear()
    {
        set = 0;
    }

    bool empty() const
    {
        return set == 0;
    }

    inline bool operator==(const CUnitMap<T>& other) const
    {
        if (set != other.set)
            return false;
        for (int i = 0; i < UNIT_COUNT; i++)
            if (HasIndex(i) && data[i] != other.data[i])
                return false;
        return true;
    }

    inline bool operator!=(const CUnitMap<T>& other) const
    {
        return !(*this == other);
    }
};

#endif
