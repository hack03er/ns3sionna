/*
 * Copyright (c) 2024 Yannik Pilz
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Yannik Pilz <y.pilz@campus.tu-berlin.de>
 */

#ifndef SIONNA_PROPAGATION_CACHE_H
#define SIONNA_PROPAGATION_CACHE_H
 
#include "ns3/mobility-model.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include "sionna-helper.h"

#include <map>

#include <ns3/propagation-delay-model.h>
#include "ns3/propagation-loss-model.h"

namespace ns3
{

class SionnaPropagationCache : public ns3::Object
{
    public:
        static TypeId GetTypeId();

        SionnaPropagationCache();
        ~SionnaPropagationCache();

        Time GetPropagationDelay(Ptr<MobilityModel> a, Ptr<MobilityModel> b) const;
        double GetPropagationLoss(Ptr<MobilityModel> a, Ptr<MobilityModel> b, double txPowerDbm) const;
        void SetSionnaHelper(SionnaHelper &sionnaHelper);
        void SetCaching(bool caching);
        void SetOptimize(bool optimize);
        double GetStats();

    private:
        struct CacheKey
        {
            CacheKey(uint32_t a, uint32_t b)
                : m_first(a < b ? a : b),
                  m_second(a < b ? b : a)
            {
            }

            uint32_t m_first;
            uint32_t m_second;

            bool operator<(const CacheKey& other) const
            {
                if (m_first != other.m_first)
                {
                    return m_first < other.m_first;
                }
                else
                {
                    return m_second < other.m_second;
                }
            }
        };

        struct CacheEntry
        {
            CacheEntry(Time delay, double loss, Time start_time, Time end_time)
                : m_delay(delay),
                  m_loss(loss),
                  m_start_time(start_time),
                  m_end_time(end_time)
            {
            }
            
            Time m_delay;
            double m_loss;
            Time m_start_time;
            Time m_end_time;
        };

        CacheEntry GetPropagationData(Ptr<MobilityModel> a, Ptr<MobilityModel> b) const;

        SionnaHelper *m_sionnaHelper;
        bool m_caching;
        typedef std::map<CacheKey, std::vector<CacheEntry>> Cache;
        mutable Cache m_cache;
        mutable double m_cache_hits;
        mutable double m_cache_miss;
        bool m_optimize; // too far distance are not computed with raytracing
        const double m_optimize_margin = 0;
        Ptr<FriisPropagationLossModel> m_friisLossModel;
        Ptr<ConstantSpeedPropagationDelayModel> m_constSpeedDelayModel;
};

} // namespace ns3
 
#endif // SIONNA_PROPAGATION_CACHE_H
