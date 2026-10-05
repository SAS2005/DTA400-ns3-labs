#include "ns3/core-module.h"

#include <iomanip>
#include <queue>

using namespace ns3;

struct Stats
{
    uint64_t arrivals = 0, served = 0, lost = 0;
    double totalWait = 0.0, totalSystem = 0.0, totalService = 0.0;
    double areaLq = 0.0, areaL = 0.0, areaBusy = 0.0; // time integrals
    double lastChange = 0.0;
};

class CarWash
{
  public:
    CarWash(uint32_t c,
            double lambda,
            double mu,
            double simTime,
            int64_t seed,
            uint32_t maxQueue = UINT32_MAX)
        : m_c(c),
          m_lambda(lambda),
          m_mu(mu),
          m_simTime(simTime),
          m_maxQueue(maxQueue)
    {
        RngSeedManager::SetSeed(seed >= 0 ? (uint64_t)seed : 12345);
        m_interArrival = CreateObject<ExponentialRandomVariable>(); // mean = 1/lambda
        m_interArrival->SetAttribute("Mean", DoubleValue(1.0 / m_lambda));
        m_service = CreateObject<ExponentialRandomVariable>(); // mean = 1/mu
        m_service->SetAttribute("Mean", DoubleValue(1.0 / m_mu));
    }

    void Run()
    {
        Simulator::Schedule(Seconds(m_interArrival->GetValue()), &CarWash::OnArrival, this);
        Simulator::Stop(Seconds(m_simTime));
        Simulator::Run();
        Simulator::Destroy();
        Report();
    }

  private:
    void UpdateAreas()
    {
        double now = Simulator::Now().GetSeconds();
        double dt = now - m_stats.lastChange;
        // Lq = queue size; L = queue + busy servers
        m_stats.areaLq += dt * m_queue.size();
        m_stats.areaL += dt * (m_queue.size() + m_busy);
        m_stats.areaBusy += dt * m_busy;
        m_stats.lastChange = now;
    }

    void OnArrival()
    {
        double now = Simulator::Now().GetSeconds();
        UpdateAreas();
        m_stats.arrivals++;

        if (m_busy < m_c)
        {
            // Start service immediately
            m_busy++;
            double svc = m_service->GetValue();
            m_stats.totalService += svc;
            m_stats.totalSystem += svc; // wait=0
            Simulator::Schedule(Seconds(svc), &CarWash::OnDeparture, this, now);
        }
        else if (m_queue.size() < m_maxQueue)
        {
            m_queue.push(now); // enqueue with arrival timestamp
        }
        else
        {
            m_stats.lost++; // balk due to full queue
        }

        // Schedule next arrival
        double next = m_interArrival->GetValue();
        double tNext = now + next;
        if (tNext < m_simTime)
        {
            Simulator::Schedule(Seconds(next), &CarWash::OnArrival, this);
        }
    }

    void OnDeparture(double startOrArrivalTime)
    {
        double now = Simulator::Now().GetSeconds();
        UpdateAreas();

        // A server just finished service; see if queue has waiting car
        if (!m_queue.empty())
        {
            double arr = m_queue.front();
            m_queue.pop();
            double wait = now - arr;
            double svc = m_service->GetValue();
            m_stats.totalWait += wait;
            m_stats.totalService += svc;
            m_stats.totalSystem += (wait + svc);
            m_stats.served++;
            // same server continues; busy count unchanged
            Simulator::Schedule(Seconds(svc), &CarWash::OnDeparture, this, arr);
        }
        else
        {
            // No one waiting; server becomes idle
            m_stats.served++;
            m_busy--;
        }
    }

    void Report() const
    {
        double T = m_simTime;
        double lambdaEff = (double)m_stats.served / T; // throughput
        double Lq = m_stats.areaLq / T;
        double L = m_stats.areaL / T;
        double Wq = m_stats.served ? m_stats.totalWait / m_stats.served : 0.0;
        double W = m_stats.served ? m_stats.totalSystem / m_stats.served : 0.0;
        double U = (m_c > 0) ? (m_stats.areaBusy / (T * m_c)) : 0.0;
        std::cout.setf(std::ios::fixed);
        std::cout << std::setprecision(6);

        std::cout << "=== Car Wash M/M/" << m_c << " Results ===\n";
        std::cout << "Sim time (s):          " << T << "\n";
        std::cout << "λ (arrivals/s):        " << m_lambda << "\n";
        std::cout << "μ (services/s):        " << m_mu << "\n";
        std::cout << "Servers c:             " << m_c << "\n";
        if (m_maxQueue != UINT32_MAX)
        {
            std::cout << "Max queue:             " << m_maxQueue << "\n";
        }

        std::cout << "Arrivals:              " << m_stats.arrivals << "\n";
        std::cout << "Served:                " << m_stats.served << "\n";
        std::cout << "Lost (full queue):     " << m_stats.lost << "\n";
        std::cout << "Throughput λ_eff:      " << lambdaEff << " /s\n";
        std::cout << "Utilization ρ:         " << U << "\n";
        std::cout << "E[Lq] (avg queue):     " << Lq << "\n";
        std::cout << "E[L]  (in system):     " << L << "\n";
        std::cout << "E[Wq] (wait time):     " << Wq << " s\n";
        std::cout << "E[W]  (system time):   " << W << " s\n";
    }

    // Parameters
    uint32_t m_c;
    double m_lambda, m_mu, m_simTime;
    uint32_t m_maxQueue;

    // State
    uint32_t m_busy = 0;
    std::queue<double> m_queue;
    Stats m_stats;

    // RNGs
    Ptr<ExponentialRandomVariable> m_interArrival;
    Ptr<ExponentialRandomVariable> m_service;
};

int
main(int argc, char** argv)
{
    uint32_t c = 2;
    double lambda = 0.9;            // arrivals per second
    double mu = 0.6;                // services per second per server
    double simTime = 3600;          // seconds
    uint32_t maxQueue = UINT32_MAX; // infinite by default
    int64_t seed = 42;

    CommandLine cmd(__FILE__);
    cmd.AddValue("c", "Number of parallel servers", c);
    cmd.AddValue("lambda", "Arrival rate (per second)", lambda);
    cmd.AddValue("mu", "Service rate per server (per second)", mu);
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.AddValue("maxQueue", "Max queue length (UINT32_MAX for infinite)", maxQueue);
    cmd.AddValue("seed", "RNG seed", seed);
    cmd.Parse(argc, argv);

    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::DefaultSimulatorImpl"));
    Time::SetResolution(Time::NS);

    CarWash model(c, lambda, mu, simTime, seed, maxQueue);
    model.Run();
    return 0;
}
