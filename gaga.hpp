// Gaga: lightweight simple genetic algorithm library
// Copyright (c) Jean Disset 2019, All rights reserved.

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3.0 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library.

/******************************************************************************************
 *                                 GAGA LIBRARY
 *****************************************************************************************/
// This file contains :
// 1 - the Individual class template : an individual's generic representation, with its
// dna, fitnesses and behavior footprints (for novelty)
// 2 - the main GA class template

#ifndef GAMULTI_HPP
#define GAMULTI_HPP

#include <assert.h>
#include <sys/stat.h>
//#include <sys/types.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "third_party/cxxpool.hpp"
#include "third_party/json.hpp"

#define GAGA_COLOR_PURPLE "\033[35m"
#define GAGA_COLOR_PURPLEBOLD "\033[1;35m"
#define GAGA_COLOR_BLUE "\033[34m"
#define GAGA_COLOR_BLUEBOLD "\033[1;34m"
#define GAGA_COLOR_GREY "\033[30m"
#define GAGA_COLOR_GREYBOLD "\033[1;30m"
#define GAGA_COLOR_YELLOW "\033[33m"
#define GAGA_COLOR_YELLOWBOLD "\033[1;33m"
#define GAGA_COLOR_RED "\033[31m"
#define GAGA_COLOR_REDBOLD "\033[1;31m"
#define GAGA_COLOR_CYAN "\033[36m"
#define GAGA_COLOR_CYANBOLD "\033[1;36m"
#define GAGA_COLOR_GREEN "\033[32m"
#define GAGA_COLOR_GREENBOLD "\033[1;32m"
#define GAGA_COLOR_NORMAL "\033[0m"

#ifdef GAGA_TESTING
#define GAGA_PROTECTED_TESTABLE public
#else
#define GAGA_PROTECTED_TESTABLE protected
#endif

namespace GAGA {

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::unordered_map;
using std::unordered_set;
using simpleVec = std::vector<double>;  // footprints for novelty
using json = nlohmann::json;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using std::chrono::system_clock;

template <typename T, typename U>
std::ostream &operator<<(std::ostream &out, const std::pair<T, U> &p) {
	out << "{" << p.first << ", " << p.second << "}";
	return out;
}

/*****************************************************************************
 *                         INDIVIDUAL CLASS
 * **************************************************************************/
// - wrapper for a dna
// - stores fitness + footprint (for comparison during novelty search)
// - stores varioous infos (stats, lineage, custom infos)
//
// **************************************************************************
// A valid DNA class MUST have:
// ----------------------------
// string serialize() # MANDATORY
// constructor from serialized string # MANDATORY
//
// A DNA class SHOULD have:
// ------------------------
// DNA mutate() # optional (but you won't do much without it)
// DNA crossover(DNA& other) # optional for mutation only search
//
// A DNA class CAN have:
// ---------------------
// void reset() # if exists, will be used between each reuse of the same dna
//
// **************************************************************************
template <typename DNA, typename F = simpleVec> struct Individual {
	using footprint_t = F;
	using id_t = std::pair<size_t, size_t>;

	DNA dna;

	std::map<std::string, double>
	    fitnesses;          // std::map {"fitnessCriterName" -> "fitnessValue"}
	footprint_t footprint;  // individual's footprint for novelty computation
	bool evaluated = false;
	bool wasAlreadyEvaluated = false;
	double evalTime = 0.0;
	id_t id{0u, 0u};  // gen id , ind id

	std::string infos;  // custom infos, description, whatever... filled by user
	std::map<string, double> stats;  // custom stats, filled by user
	// ancestry & lineage
	std::vector<std::pair<size_t, size_t>>
	    parents;  // vector of {{ generation id, individual id }}
	std::string inheritanceType =
	    "exnihilo";  // inheritance type : mutation, crossover, copy, exnihilo

	// Individual() {}
	explicit Individual(const DNA &d) : dna(d) {}

	explicit Individual(const json &o) {
		assert(o.count("dna"));
		dna = DNA(o.at("dna").get<std::string>());
		if (o.count("footprint")) footprint = o.at("footprint").get<footprint_t>();
		if (o.count("fitnesses")) fitnesses = o.at("fitnesses").get<decltype(fitnesses)>();
		if (o.count("infos")) infos = o.at("infos");
		if (o.count("evaluated")) evaluated = o.at("evaluated");
		if (o.count("alreadyEval")) wasAlreadyEvaluated = o.at("alreadyEval");
		if (o.count("evalTime")) evalTime = o.at("evalTime");
		if (o.count("stats")) stats = o.at("stats").get<decltype(stats)>();
		if (o.count("parents")) parents = o.at("parents").get<decltype(parents)>();
		if (o.count("inheritanceType")) inheritanceType = o.at("inheritanceType");
		if (o.count("id")) id = o.at("id").get<decltype(id)>();
	}

	// Exports individual to json
	json toJSON() const {
		json o;
		o["dna"] = dna.serialize();
		o["fitnesses"] = fitnesses;
		o["footprint"] = footprint;
		o["infos"] = infos;
		o["evaluated"] = evaluated;
		o["alreadyEval"] = wasAlreadyEvaluated;
		o["evalTime"] = evalTime;
		o["stats"] = stats;
		o["parents"] = parents;
		o["inheritanceType"] = inheritanceType;
		o["id"] = id;
		return o;
	}

	// Exports a std::vector of individual to json
	static json popToJSON(const std::vector<Individual<DNA, footprint_t>> &p) {
		json o;
		json popArray;
		for (auto &i : p) popArray.push_back(i.toJSON());
		o["population"] = popArray;
		return o;
	}

	// Loads a std::vector of individual from json
	static std::vector<Individual<DNA, footprint_t>> loadPopFromJSON(const json &o) {
		assert(o.count("population"));
		std::vector<Individual<DNA, footprint_t>> res;
		json popArray = o.at("population");
		for (auto &ind : popArray) res.push_back(Individual<DNA, footprint_t>(ind));
		return res;
	}
};

template <typename DNA, typename f>
void to_json(nlohmann::json &j, const Individual<DNA, f> &i) {
	j = i.toJSON();
}

template <typename DNA, typename f>
void from_json(const nlohmann::json &j, Individual<DNA, f> &i) {
	i = Individual<DNA, f>(j);
}

/*********************************************************************************
 *                                 GA CLASS
 ********************************************************************************/
// DNA requirements : see Individual class;
//
// Evaluator class requirements (see examples folder):
// constructor(int argc, char** argv)
// void operator()(const Ind_t& ind)
// const string name
//
// TYPICAL USAGE :
//
// GA<DNAType, EvalType> ga;
// ga.setPopSize(400);
// return ga.start();

enum class SelectionMethod { paretoTournament, randomObjTournament };
template <typename DNA, typename Fp = simpleVec> class GA {
 public:
	using footprint_t = Fp;
	using Ind_t = Individual<DNA, footprint_t>;
	using Iptr = Ind_t *;
	using DNA_t = DNA;
	using distanceMatrix_t = std::vector<std::vector<double>>;

	GAGA_PROTECTED_TESTABLE :

	    /*********************************************************************************
	     *                            MAIN GA SETTINGS
	     ********************************************************************************/
	    unsigned int verbosity = 2;       // 0 = silent; 1 = generations stats;
	                                      // 2 = individuals stats; 3 = everything
	size_t popSize = 500;                 // nb of individuals in the population
	size_t nbElites = 1;                  // nb of elites to keep accross generations
	size_t nbSavedElites = 1;             // nb of elites to save
	size_t tournamentSize = 5;            // nb of competitors in tournament
	bool savePopEnabled = true;           // save the whole population
	unsigned int savePopInterval = 1;     // interval between 2 whole population saves
	unsigned int saveGenInterval = 1;     // interval between 2 elites/pareto saves
	string folder = "../evos/";           // where to save the results
	string evaluatorName;                 // name of the given evaluator func
	double crossoverRate = 0.2;           // crossover probability
	double mutationRate = 0.5;            // mutation probablility
	bool evaluateAllIndividuals = false;  // force evaluation of every individual
	bool doSaveParetoFront = false;       // save the pareto front
	bool doSaveGenStats = true;           // save generations stats to csv file
	bool doSaveIndStats = false;          // save individuals stats to csv file
	SelectionMethod selecMethod = SelectionMethod::paretoTournament;

	// for novelty:
	bool novelty = false;            // enable novelty
	bool nslc = false;               // enable noverlty search with local competition
	std::vector<Ind_t> archive;      // when novelty is enabled, we store individuals there
	size_t KNN = 5;                  // size of the neighbourhood for novelty
	bool saveArchiveEnabled = true;  // save the novelty archive
	size_t nbOfArchiveAdditionsPerGeneration = 5;
	std::function<double(const footprint_t &, const footprint_t)> computeFootprintDistance =
	    [](const auto &, const auto &) { return 0; };
	std::function<distanceMatrix_t(const std::vector<Ind_t> &)> computeDistanceMatrix =
	    [this](const auto &ar) { return this->defaultComputeDistanceMatrix(ar); };

	// for speciation:
	bool speciation = false;           // enable speciation
	double speciationThreshold = 0.2;  // min distance between two dna of same specie
	size_t minSpecieSize = 15;         // minimum specie size
	double minSpeciationThreshold = 0.03;
	double maxSpeciationThreshold = 0.5;
	double speciationThresholdIncrement = 0.01;
	std::function<double(const Individual<DNA, footprint_t> &,
	                     const Individual<DNA, footprint_t> &)>
	    indDistanceFunction = [](const auto &, const auto &) { return 0.0; };
	const unsigned int MAX_SPECIATION_TRIES = 100;
	std::vector<double> speciationThresholds;  // spec thresholds per specie

	// thread pool
	unsigned int nbThreads = 1;
	cxxpool::thread_pool tp{nbThreads};

	/********************************************************************************
	 *                                 SETTERS
	 ********************************************************************************/
 public:
	void enablePopulationSave() { savePopEnabled = true; }
	void disablePopulationSave() { savePopEnabled = false; }
	void enableArchiveSave() { saveArchiveEnabled = true; }
	void disableArchiveSave() { saveArchiveEnabled = false; }
	const std::vector<Ind_t> &getArchive() const { return archive; }
	void setVerbosity(unsigned int lvl) { verbosity = lvl <= 3 ? (lvl >= 0 ? lvl : 0) : 3; }
	void setPopSize(size_t s) { popSize = s; }
	size_t getPopSize() { return popSize; }
	void setNbElites(size_t n) { nbElites = n; }
	size_t getNbElites() { return nbElites; }
	void setNbSavedElites(size_t n) { nbSavedElites = n; }
	void setTournamentSize(size_t n) { tournamentSize = n; }
	void setPopSaveInterval(unsigned int n) { savePopInterval = n; }
	void setGenSaveInterval(unsigned int n) { saveGenInterval = n; }
	void setSaveFolder(string s) { folder = s; }
	string getSaveFolder() { return folder; }
	void setNbThreads(unsigned int n) {
		if (n >= nbThreads)
			tp.add_threads(n - nbThreads);
		else {
			std::cerr << "cannot remove threads from thread pool" << std::endl;
		}
		nbThreads = n;
	}
	void setCrossoverRate(double p) {
		crossoverRate = p <= 1.0 ? (p >= 0.0 ? p : 0.0) : 1.0;
	}
	double getCrossoverRate() { return crossoverRate; }
	void setMutationRate(double p) { mutationRate = p <= 1.0 ? (p >= 0.0 ? p : 0.0) : 1.0; }
	int getVerbosity() { return verbosity; }
	double getMutationRate() { return mutationRate; }
	void setEvaluator(std::function<void(Ind_t &, int)> e,
	                  std::string ename = "anonymousEvaluator") {
		evaluator = e;
		evaluatorName = ename;
		for (auto &i : population) {
			i.evaluated = false;
			i.wasAlreadyEvaluated = false;
		}
	}

	void setMutateMethod(std::function<void(DNA_t &)> m) { mutate = m; }
	void setCrossoverMethod(std::function<DNA_t(const DNA_t &, const DNA_t &)> m) {
		crossover = m;
	}

	void setNewGenerationFunction(std::function<void(void)> f) {
		newGenerationFunction = f;
	}  // called before evaluating the current population

	void setEvaluateFunction(std::function<void(void)> f) {
		evaluate = f;
	}  // evaluation of the whole population

	void setNextGenerationFunction(std::function<void(void)> f) {
		nextGeneration = f;
	}  // evaluation and next generation

	void setIsBetterMethod(std::function<bool(double, double)> f) { isBetter = f; }
	void setSelectionMethod(const SelectionMethod &sm) { selecMethod = sm; }

	using objList_t = std::unordered_set<std::string>;
	template <typename S> std::function<Iptr(S &, const objList_t &)> getSelectionMethod() {
		switch (selecMethod) {
			case SelectionMethod::paretoTournament:
				return [this](S &subPop, const objList_t &objectives) {
					return paretoTournament(subPop, objectives);
				};
			case SelectionMethod::randomObjTournament:
			default:
				return [this](S &subPop, const objList_t &objectives) {
					return randomObjTournament(subPop, objectives);
				};
		}
	}

	bool getEvaluateAllIndividuals() { return evaluateAllIndividuals; }
	void setEvaluateAllIndividuals(bool m) { evaluateAllIndividuals = m; }
	void setSaveParetoFront(bool m) { doSaveParetoFront = m; }
	void setSaveGenStats(bool m) { doSaveGenStats = m; }
	void setSaveIndStats(bool m) { doSaveIndStats = m; }

	// main current and previous population containers
	std::vector<Ind_t> population;  // current population
	std::vector<std::vector<Ind_t>> previousGenerations;

	// for novelty:
	void enableNovelty() { novelty = true; }
	void disableNovelty() { novelty = false; }
	bool noveltyEnabled() const { return novelty; }
	void setKNN(size_t n) { KNN = n; }
	size_t getKNN() { return KNN; }
	template <typename F> void setComputeFootprintDistanceFunction(F &&f) {
		computeFootprintDistance = std::forward<F>(f);
	}
	template <typename F> void setComputDistanceMatrixFunction(F &&f) {
		computeDistanceMatrix = std::forward<F>(f);
	}

	void setNbOfArchiveAdditionsPerGeneration(size_t n) {
		nbOfArchiveAdditionsPerGeneration = n;
	}

	// for speciation:
	void enableSpeciation() {
		nextGeneration = [this]() { speciationNextGen(); };
		speciation = true;
	}
	void disableSpeciation() {
		nextGeneration = [this]() { classicNextGen(); };
		speciation = false;
	}

	bool speciationEnabled() { return speciation; }
	void setMinSpeciationThreshold(double s) { minSpeciationThreshold = s; }
	double getMinSpeciationThreshold() { return minSpeciationThreshold; }
	void setMaxSpeciationThreshold(double s) { maxSpeciationThreshold = s; }
	double getMaxSpeciationThreshold() { return maxSpeciationThreshold; }
	void setSpeciationThreshold(double s) { speciationThreshold = s; }
	double getSpeciationThreshold() { return speciationThreshold; }
	void setSpeciationThresholdIncrement(double s) { speciationThresholdIncrement = s; }
	double getSpeciationThresholdIncrement() { return speciationThresholdIncrement; }
	void setMinSpecieSize(double s) { minSpecieSize = s; }
	double getMinSpecieSize() { return minSpecieSize; }
	void setIndDistanceFunction(std::function<double(const Ind_t &, const Ind_t &)> f) {
		indDistanceFunction = f;
	}
	size_t getCurrentGenerationNumber() const { return currentGeneration; }

	std::vector<std::vector<Iptr>> species;  // pointers to the individuals of the species

	// genStats will be populated with some basic generation stats
	std::vector<std::map<std::string, std::map<std::string, double>>> genStats;

	////////////////////////////////////////////////////////////////////////////////////

	std::random_device rd;
	std::default_random_engine globalRand = std::default_random_engine(rd());

	GAGA_PROTECTED_TESTABLE :

	    size_t currentGeneration = 0;
	bool customInit = false;
	int procId = 0;
	int nbProcs = 1;

	// default mutate and crossover are taken from the DNA_t class, if they are defined.
	template <class D> auto defaultMutate(D &d) -> decltype(d.mutate()) {
		return d.mutate();
	}
	template <class D, class... SFINAE> void defaultMutate(D &d, SFINAE...) {
		printLn(3, "WARNING: no mutate method specified");
	}

	template <class D>
	auto defaultCrossover(const D &d1, const D &d2) -> decltype(d1.crossover(d2)) {
		return d1.crossover(d2);
	}
	template <class D, class... SFINAE> D defaultCrossover(const D &d1, SFINAE...) {
		printLn(3, "WARNING: no crossover method specified");
		return d1;
	}

	// default reset method taken from the DNA_t class, if defined.
	template <class D> auto defaultReset(D &d) -> decltype(d.reset()) { return d.reset(); }
	template <class D, class... SFINAE> void defaultReset(D &d, SFINAE...) {
		printLn(3, "WARNING: no reset method specified");
	}

	// crossover and mutate are 2 lambdas that call defaultMutate and defaultCrossover. It
	// shouldn't be needed for most use cases, but for advanced usage and logging, the user
	// can override these methods. Change at your own risks.
	std::function<DNA_t(const DNA_t &, const DNA_t &)> crossover =
	    [this](const DNA_t &d1, const DNA_t &d2) { return defaultCrossover(d1, d2); };
	std::function<void(DNA_t &)> mutate = [this](DNA_t &d) { defaultMutate(d); };

	Ind_t mutatedIndividual(const Ind_t &i) {
		Ind_t offspring(i.dna);
		mutate(offspring.dna);
		offspring.parents.clear();
		offspring.parents.push_back(i.id);
		offspring.inheritanceType = "mutation";
		offspring.evaluated = false;
		return offspring;
	}

	Ind_t crossoverIndividual(const Ind_t &a, const Ind_t &b) {
		Ind_t offspring(crossover(a.dna, b.dna));
		offspring.parents.clear();
		offspring.parents.push_back(a.id);
		offspring.parents.push_back(b.id);
		offspring.inheritanceType = "crossover";
		offspring.evaluated = false;
		return offspring;
	}

	std::function<void(Ind_t &, int)> evaluator;
	std::function<void(void)> newGenerationFunction = []() {};
	std::function<void(void)> nextGeneration = [this]() { classicNextGen(); };
	std::function<void(void)> evaluate = [this]() { defaultEvaluate(); };
	std::function<bool(double, double)> isBetter = [](double a, double b) { return a > b; };

	// returns a reference (transforms pointer into reference)
	template <typename T> inline T &ref(T &obj) { return obj; }
	template <typename T> inline T &ref(T *obj) { return *obj; }
	template <typename T> inline const T &ref(const T &obj) { return obj; }
	template <typename T> inline const T &ref(const T *obj) { return *obj; }

 public:
	/*********************************************************************************
	 *                              CONSTRUCTOR
	 ********************************************************************************/
	GA() {}

	/*********************************************************************************
	 *                          START THE BOUZIN
	 ********************************************************************************/
	void setPopulation(const std::vector<Ind_t> &p) {
		if (procId == 0) {
			population = p;
			if (population.size() != popSize)
				throw std::invalid_argument("Population doesn't match the popSize param");
			popSize = population.size();
		}
		setPopulationId(population, currentGeneration);
	}

	void initPopulation(const std::function<DNA()> &f) {
		if (procId == 0) {
			population.clear();
			population.reserve(popSize);
			for (size_t i = 0; i < popSize; ++i) {
				population.push_back(Ind_t(f()));
				population[population.size() - 1].evaluated = false;
			}
		}
		setPopulationId(population, currentGeneration);
	}

	template <typename... Args> void printError(Args &&... a) {
		const size_t ERROR_VERBOSITY_LVL = 1;
		printLn(ERROR_VERBOSITY_LVL, GAGA_COLOR_RED, "[ERROR] ", GAGA_COLOR_NORMAL,
		        std::forward<Args>(a)...);
	}

	template <typename... Args> void printWarning(Args &&... a) {
		const size_t WARNING_VERBOSITY_LVL = 2;
		printLn(WARNING_VERBOSITY_LVL, GAGA_COLOR_YELLOW, "[WARNING] ", GAGA_COLOR_NORMAL,
		        std::forward<Args>(a)...);
	}

	template <typename... Args> void printLn(size_t lvl, Args &&... a) {
		if (verbosity >= lvl) {
			std::ostringstream output;
			subPrint(output, std::forward<Args>(a)...);
			std::cout << output.str();
		}
	}
	template <typename T, typename... Args>
	void subPrint(std::ostringstream &output, const T &t, Args &&... a) {
		output << t;
		subPrint(output, std::forward<Args>(a)...);
	}
	void subPrint(std::ostringstream &output) { output << std::endl; }

	void defaultEvaluate() {  // uses evaluator
		if (!evaluator) throw std::invalid_argument("No evaluator specified");
		std::vector<std::future<void>> futures;
		futures.reserve(population.size());
		for (size_t i = 0; i < population.size(); ++i) {
			futures.push_back(tp.push([&, i]() {
				if (evaluateAllIndividuals || !population[i].evaluated) {
					printLn(3, "Going to evaluate ind ");
					auto t0 = high_resolution_clock::now();
					defaultReset(population[i].dna);
					evaluator(population[i], 0);
					auto t1 = high_resolution_clock::now();
					population[i].evaluated = true;
					double indTime = std::chrono::duration<double>(t1 - t0).count();
					population[i].evalTime = indTime;
					population[i].wasAlreadyEvaluated = false;
				} else {
					population[i].evalTime = 0.0;
					population[i].wasAlreadyEvaluated = true;
				}
				if (verbosity >= 2) printIndividualStats(population[i]);
			}));
		}
		for (auto &f : futures) f.get();
	}

	// "Vroum vroum"
	void step(int nbGeneration = 1) {
		if (currentGeneration == 0 && procId == 0) {
			createFolder(folder);
			if (verbosity >= 1) printStart();
		}
		for (int nbg = 0; nbg < nbGeneration; ++nbg) {
			newGenerationFunction();
			auto tg0 = high_resolution_clock::now();
			nextGeneration();
			if (procId == 0) {  // stats
				assert(previousGenerations.back().size());
				if (population.size() != popSize)
					throw std::invalid_argument("Population doesn't match the popSize param");
				auto tg1 = high_resolution_clock::now();
				double totalTime = std::chrono::duration<double>(tg1 - tg0).count();
				auto tnp0 = high_resolution_clock::now();
				if (savePopInterval > 0 && currentGeneration % savePopInterval == 0) {
					if (savePopEnabled) savePop();
					if (novelty && saveArchiveEnabled) saveArchive();
				}
				if (saveGenInterval > 0 && currentGeneration % saveGenInterval == 0) {
					if (doSaveParetoFront) {
						saveParetoFront();
					} else {
						saveBests(nbSavedElites);
						if (nbSavedElites > 0) saveBests(nbSavedElites);
					}
				}
				updateStats(totalTime);
				if (verbosity >= 1) printGenStats(currentGeneration);
				if (doSaveGenStats) saveGenStats();
				if (doSaveIndStats) saveIndStats();
				auto tnp1 = high_resolution_clock::now();
				double tnp = std::chrono::duration<double>(tnp1 - tnp0).count();
				if (verbosity >= 2) {
					std::cout << "Time for save + next pop = " << tnp << " s." << std::endl;
				}
			}
			++currentGeneration;
		}
	}

	// helper that returns the unordered list of all objectives currently in an individual
	template <typename I>
	static std::unordered_set<std::string> getAllObjectives(const I &i) {
		std::unordered_set<std::string> objs;
		for (const auto &o : i.fitnesses) objs.insert(o.first);
		return objs;
	}
	/*********************************************************************************
	 *                            NEXT POP GETTING READY
	 ********************************************************************************/
	void classicNextGen() {
		assert(population.size() > 0);
		// simple next generation routine:
		// 1 - evaluation (wih obj + novelty fitnesses)
		// 2 - selection with mutations/crossovers
		evaluate();
		if (novelty) {
			updateNovelty();
		}
		std::unordered_set<std::string> objectives;
		if (novelty && nslc) {
			objectives.insert("novelty");
			objectives.insert("local_score");
		} else {
			objectives = getAllObjectives(population[0]);
		}
		auto nextGen = produceNOffsprings(popSize, population, nbElites, objectives);
		previousGenerations.push_back(population);
		population = nextGen;
		setPopulationId(population, currentGeneration + 1);
		if (verbosity >= 3) cerr << "Next generation ready" << endl;
	}

	void setPopulationId(std::vector<Ind_t> &p, size_t genId) {
		// reinitialize individuals' id to a pair {genId , 0 to N}
		for (size_t i = 0; i < p.size(); ++i) p[i].id = std::make_pair(genId, i);
	}

	void speciationNextGen() {
		// next generation routine with speciation
		// - evaluation de toute la pop, sans se soucier des espèces.
		// - choix des nouveaux représentants parmis les espèces précédentes (clonage)
		// - création d'une nouvelle population via selection/mutation/crossover
		// intra-espece
		// - regroupement en nouvelles espèces en utilisant la distance aux représentants
		// (création d'une nouvelle espèce si distance < speciationThreshold)
		// - on supprime les espèces de taille < minSpecieSize
		// - on rajoute des individus en les faisant muter depuis une espèce aléatoire
		// (nouvelle espèce à chaque tirage) et en les rajoutants à cette espèce
		// - c'est reparti :D

		// TODO : for now it only works with maximization
		// minimization would require a modification in the nOffsprings
		// and in the worstFitness computations

		evaluate();
		if (novelty) updateNovelty();
		std::unordered_set<std::string> objectives;
		if (novelty && nslc) {
			objectives = {{"novelty", "local_score"}};
		} else {
			objectives = getAllObjectives(population[0]);
		}

		if (verbosity >= 3) cerr << "Starting to prepare next speciated gen" << std::endl;
		assert(nbElites < minSpecieSize);

		if (species.size() == 0) {
			if (verbosity >= 3) cerr << "No specie available, creating one" << std::endl;
			// we put all the population in one species
			species.resize(1);
			for (auto &i : population) species[0].push_back(&i);
			speciationThresholds.clear();
			speciationThresholds.resize(1);
			speciationThresholds[0] = speciationThreshold;
		}

		assert(species.size() == speciationThresholds.size());

		std::vector<Ind_t> nextLeaders;
		// New species leaders
		for (auto &s : species) {
			assert(s.size() > 0);
			std::uniform_int_distribution<size_t> d(0, s.size() - 1);
			nextLeaders.push_back(*s[d(globalRand)]);
		}
		if (verbosity >= 3)
			cerr << "Found " << nextLeaders.size() << " leaders :" << std::endl;

		// list of objectives
		unordered_set<string> objectivesList;
		for (const auto &o : population[0].fitnesses) objectivesList.insert(o.first);
		assert(objectivesList.size() > 0);
		if (verbosity >= 3)
			cerr << "Found " << objectivesList.size() << " objectives" << std::endl;

		// computing afjustedFitnesses
		std::vector<unordered_map<string, double>> adjustedFitnessSum(species.size());
		unordered_map<string, double> worstFitness;
		for (const auto &o : objectivesList) {
			worstFitness[o] = std::numeric_limits<double>::max();
			for (const auto &i : population)
				if (i.fitnesses.at(o) < worstFitness.at(o)) worstFitness[o] = i.fitnesses.at(o);
		}
		// we want to offset all the adj fitnesses so they are in the positive range
		unordered_map<string, double> totalAdjustedFitness;
		for (const auto &o : objectivesList) {
			double total = 0;
			for (size_t i = 0; i < species.size(); ++i) {
				const auto &s = species[i];
				assert(s.size() > 0);
				double sum = 0;
				for (const auto &ind : s) sum += ind->fitnesses.at(o) - worstFitness.at(o) + 1;
				sum /= static_cast<double>(s.size());
				total += sum;
				adjustedFitnessSum[i][o] = sum;
			}
			totalAdjustedFitness[o] = total;
		}
		if (verbosity >= 3) {
			for (auto &af : totalAdjustedFitness) {
				cerr << " - total \"" << af.first << "\" = " << af.second << std::endl;
			}
		}

		// creating the new population
		std::vector<Ind_t> nextGen;
		for (const auto &o : objectivesList) {
			assert(totalAdjustedFitness[o] != 0);
			for (size_t i = 0; i < species.size(); ++i) {
				auto &s = species[i];
				size_t nOffsprings =  // nb of offsprings the specie is authorized to produce
				    static_cast<size_t>((static_cast<double>(popSize) /
				                         static_cast<double>(objectivesList.size())) *
				                        adjustedFitnessSum[i][o] / totalAdjustedFitness[o]);

				nOffsprings = std::max(static_cast<size_t>(nOffsprings), 1ul);
				nextGen.reserve(nextGen.size() + nOffsprings);

				auto specieOffsprings = produceNOffsprings(nOffsprings, s, nbElites);
				nextGen.insert(nextGen.end(), std::make_move_iterator(specieOffsprings.begin()),
				               std::make_move_iterator(specieOffsprings.end()));
			}
		}
		previousGenerations.push_back(population);
		population = nextGen;
		setPopulationId(population, currentGeneration + 1);

		if (verbosity >= 3)
			cerr << "Created the new population. Population.size = " << population.size()
			     << std::endl;

		// correcting rounding errors by adding missing individuals
		while (population.size() < popSize) {  // we just add mutated leaders
			std::uniform_int_distribution<size_t> d(0, nextLeaders.size() - 1);
			population.push_back(mutatiedIndividual(nextLeaders[d(globalRand)]));
		}
		while (population.size() > popSize) population.pop_back();  // or delete the extra

		assert(population.size() == popSize);

		// reevaluating the new guys
		evaluate();
		if (novelty) updateNovelty();

		// creating new species
		species.clear();
		species.resize(nextLeaders.size());
		assert(species.size() > 0);
		for (auto &i : population) {
			// finding the closest leader
			size_t closestLeader = 0;
			double closestDist = std::numeric_limits<double>::max();
			bool foundSpecie = false;
			std::vector<double> distances(nextLeaders.size());

			std::vector<std::future<void>> futures;
			for (size_t l = 0; l < nextLeaders.size(); ++l)
				futures.push_back(tp.push([=, &distances, &nextLeaders]() {
					distances[l] = indDistanceFunction(nextLeaders[l], i);
				}));
			for (auto &f : futures) f.get();

			for (size_t d = 0; d < distances.size(); ++d) {
				if (distances[d] < closestDist && distances[d] < speciationThresholds[d]) {
					closestDist = distances[d];
					closestLeader = d;
					foundSpecie = true;
				}
			}
			if (foundSpecie) {
				// we found your family
				species[closestLeader].push_back(&i);
			} else {
				// we found special snowflakes
				nextLeaders.push_back(i);
				species.push_back({{&i}});
				speciationThresholds.push_back(speciationThreshold);
			}
		}
		if (verbosity >= 3)
			cerr << "Created the new species. Species size = " << species.size() << std::endl;

		assert(species.size() > 0);
		assert(species.size() == nextLeaders.size());
		assert(species.size() == speciationThresholds.size());

		// deleting small species
		std::vector<Iptr>
		    toReplace;  // list of individuals without specie. We need to replace
		                // them with new individuals. We use this because we
		                // cannot directly delete individuals from the population
		                // without invalidating all other pointers;
		size_t cpt = 0;

		if (verbosity >= 3) {
			cerr << "Species sizes : " << std::endl;
			for (auto &s : species) {
				cerr << " - " << s.size() << std::endl;
			}
		}

		for (auto it = species.begin(); it != species.end();) {
			if ((*it).size() < minSpecieSize && species.size() > 1) {
				for (auto &i : *it) toReplace.push_back(i);
				it = species.erase(it);
				nextLeaders.erase(nextLeaders.begin() + cpt);
				speciationThresholds.erase(speciationThresholds.begin() + cpt);
			} else {
				++it;
				++cpt;
			}
		}

		assert(species.size() > 0);
		assert(species.size() == nextLeaders.size());
		assert(species.size() == speciationThresholds.size());
		assert(species.size() <= popSize / minSpecieSize);

		if (verbosity >= 3) {
			cerr << "Need to replace " << toReplace.size() << " individuals" << std::endl;
			for (auto &i : toReplace) {
				cerr << " : " << i << ", f = " << i->fitnesses.size() << std::endl;
			}
		}

		std::vector<std::future<void>> futures;
		// replacing all "deleted" individuals and putting them in existing species
		for (size_t tr = 0; tr < toReplace.size(); ++tr) {
			futures.push_back(tp.push([&, tr]() {
				auto &i = toReplace[tr];
				// we choose one random specie and mutate
				// individuals until the new ind can fit
				auto selection = getSelectionMethod<std::vector<Iptr>>();
				std::uniform_int_distribution<size_t> d(0, nextLeaders.size() - 1);
				size_t leaderID = d(globalRand);
				unsigned int c = 0;
				do {
					if (c++ > MAX_SPECIATION_TRIES)
						throw std::runtime_error("Too many tries. Speciation thresholds too low.");

					// TODO selection is not ideal here. The species hasnt been entirely evaluated.
					*i = mutatedIndividual(*selection(species[leaderID]));
				} while (indDistanceFunction(*i, nextLeaders[leaderID]) >
				         speciationThresholds[leaderID]);
			}));
		}
		for (auto &f : futures) f.get();

		if (verbosity >= 3) cerr << "Done. " << std::endl;
		// adjusting speciation Thresholds
		size_t avgSpecieSize = 0;
		for (auto &s : species) avgSpecieSize += s.size();
		avgSpecieSize /= species.size();
		for (size_t i = 0; i < species.size(); ++i) {
			if (species[i].size() < avgSpecieSize) {
				speciationThresholds[i] =
				    std::min(speciationThresholds[i] + speciationThresholdIncrement,
				             maxSpeciationThreshold);
			} else {
				speciationThresholds[i] =
				    std::max(speciationThresholds[i] - speciationThresholdIncrement,
				             minSpeciationThreshold);
			}
		}
		if (verbosity >= 3) {
			cerr << "Speciation thresholds adjusted: " << std::endl;
			for (auto &s : speciationThresholds) {
				cerr << " " << s;
			}
			cerr << std::endl;
			cerr << "Species sizes : " << std::endl;
			for (auto &s : species) {
				cerr << " - " << s.size() << std::endl;
			}
		}

		setPopulationId(population, currentGeneration + 1);
	}

	template <typename I>  // I is ither Ind_t or Ind_t*
	std::vector<Ind_t> produceNOffsprings(
	    size_t n, std::vector<I> &popu, size_t nElites,
	    const std::unordered_set<std::string> objectives) {
		assert(popu.size() >= nElites);
		assert(objectives.size() > 0);
		if (verbosity >= 3)
			cerr << "Going to produce " << n << " offsprings out of " << popu.size()
			     << " individuals" << endl;
		std::uniform_real_distribution<double> d(0.0, 1.0);
		std::vector<Ind_t> nextGen;
		nextGen.reserve(n);
		// Elites are placed at the begining
		if (nElites > 0) {
			auto elites = getElites(nElites, popu);
			if (verbosity >= 3) cerr << "elites.size = " << elites.size() << endl;
			for (auto &e : elites)
				for (auto i : e.second) {
					i.parents.clear();
					i.parents.push_back(i.id);
					i.inheritanceType = "copy";
					nextGen.push_back(i);
				}
		}

		auto selection = getSelectionMethod<std::vector<I>>();

		auto s = nextGen.size();

		size_t nCross = crossoverRate * (n - s);
		size_t nMut = mutationRate * (n - s);
		nextGen.reserve(nCross + nMut);
		std::mutex nextGenMutex;

		std::vector<std::future<void>> futures;
		for (size_t i = s; i < nCross + s; ++i) {
			futures.push_back(tp.push([&]() {
				auto offspring = crossoverIndividual(*selection(popu, objectives),
				                                     *selection(popu, objectives));
				std::lock_guard<std::mutex> thread_lock(nextGenMutex);
				nextGen.push_back(offspring);
			}));
		}

		for (size_t i = nCross + s; i < nMut + nCross + s; ++i) {
			futures.push_back(tp.push([&]() {
				auto ind = mutatedIndividual(*selection(popu, objectives));
				std::lock_guard<std::mutex> thread_lock(nextGenMutex);
				nextGen.push_back(ind);
			}));
		}

		for (auto &f : futures) f.get();

		while (nextGen.size() < n) {
			auto i = *selection(popu, objectives);
			i.parents.clear();
			i.parents.push_back(i.id);
			i.inheritanceType = "copy";
			nextGen.push_back(i);
		}

		assert(nextGen.size() == n);
		return nextGen;
	}

	// PARETO HELPERS

	bool paretoDominates(const Ind_t &a, const Ind_t &b,
	                     const std::unordered_set<std::string> &objectives) const {
		for (const auto &o : objectives) {
			assert(a.fitnesses.count(o));
			assert(b.fitnesses.count(o));
			if (!isBetter(a.fitnesses.at(o), b.fitnesses.at(o))) return false;
		}
		return true;
	}

	size_t getParetoRank(const std::vector<Ind_t *> &inds, size_t i,
	                     const std::unordered_set<std::string> &objectives) {
		return getParetoRank_recursiveImpl(inds, inds[i], objectives, 0);
	}
	size_t getParetoRank_recursiveImpl(const std::vector<Ind_t *> &inds, Ind_t *i,
	                                   const std::unordered_set<std::string> &objectives,
	                                   size_t d) {
		if (std::find(inds.begin(), inds.end(), i) == inds.end()) {
			return d;
		} else
			return getParetoRank_recursiveImpl(removeParetoFront(inds, objectives), i,
			                                   objectives, d + 1);
	}

	std::vector<Ind_t *> removeParetoFront(
	    const std::vector<Ind_t *> &ind,
	    const std::unordered_set<std::string> &objectives) const {
		// removes the pareto front and returns the rest
		auto paretoFront = getParetoFront(ind, objectives);
		std::vector<Ind_t *> res;
		res.reserve(ind.size() - paretoFront.size());
		for (auto i : ind) {
			if (std::find(paretoFront.begin(), paretoFront.end(), i) ==
			    paretoFront.end())  // not in pareto front
				res.push_back(i);
		}
		return res;
	}

	std::vector<Ind_t *> getParetoFront(const std::vector<Ind_t *> &ind) const {
		assert(ind.size() > 0);
		std::unordered_set<std::string> objs;
		for (const auto &o : ind[0]->fitnesses) objs.insert(o.first);
		return getParetoFront(ind, objs);
	}

	std::vector<Ind_t *> getParetoFront(
	    const std::vector<Ind_t *> &ind,
	    const std::unordered_set<std::string> &objectives) const {
		// naive algorithm. Should be ok for small ind.size()
		assert(ind.size() > 0);
		std::vector<Ind_t *> pareto;
		for (size_t i = 0; i < ind.size(); ++i) {
			bool dominated = false;
			for (auto &j : pareto) {
				// is i dominated by any individual already on the pareto front?
				if (paretoDominates(*j, *ind[i], objectives)) {
					dominated = true;
					break;
				}
			}
			if (!dominated) {
				for (size_t j = i + 1; j < ind.size(); ++j) {
					// or by any other point ?
					if (paretoDominates(*ind[j], *ind[i], objectives)) {
						dominated = true;
						break;
					}
				}
				if (!dominated) {
					pareto.push_back(ind[i]);
				}
			}
		}
		return pareto;
	}

	template <typename I>
	Ind_t *paretoTournament(std::vector<I> &subPop,
	                        const std::unordered_set<std::string> &objectives) {
		assert(subPop.size() > 0);
		assert(objectives.size() > 0);
		std::uniform_int_distribution<size_t> dint(0, subPop.size() - 1);
		std::vector<Ind_t *> participants;
		for (size_t i = 0; i < tournamentSize; ++i)
			participants.push_back(&ref(subPop[dint(globalRand)]));
		auto pf = getParetoFront(participants, objectives);
		assert(pf.size() > 0);
		std::uniform_int_distribution<size_t> dpf(0, pf.size() - 1);
		return pf[dpf(globalRand)];
	}

	template <typename I>
	Ind_t *randomObjTournament(std::vector<I> &subPop,
	                           const std::unordered_set<std::string> &objectives) {
		assert(subPop.size() > 0);
		assert(objectives.size() > 0);
		if (verbosity >= 3) cerr << "random obj tournament called" << endl;
		std::uniform_int_distribution<size_t> dint(0, subPop.size() - 1);
		std::vector<Ind_t *> participants;
		for (size_t i = 0; i < tournamentSize; ++i)
			participants.push_back(&ref(subPop[dint(globalRand)]));
		auto champion = participants[0];
		// we pick the objective randomly
		std::string obj;
		if (objectives.size() == 1) {
			obj = *(objectives.begin());
		} else {
			std::uniform_int_distribution<int> dObj(0, static_cast<int>(objectives.size()) - 1);
			auto it = objectives.begin();
			std::advance(it, dObj(globalRand));
			obj = *it;
		}
		for (size_t i = 1; i < tournamentSize; ++i) {
			assert(participants[i]->fitnesses.count(obj));
			if (isBetter(participants[i]->fitnesses.at(obj), champion->fitnesses.at(obj)))
				champion = participants[i];
		}
		if (verbosity >= 3) cerr << "champion found" << endl;
		return champion;
	}

	// getELites methods : returns a std::vector of N best individuals in the specified
	// subPopulations, for the specified fitnesses.
	// elites indivuduals are not ordered.
	unordered_map<string, std::vector<Ind_t>> getElites(size_t n) {
		std::vector<string> obj;
		for (auto &o : population[0].fitnesses) obj.push_back(o.first);
		return getElites(obj, n, population);
	}

	template <typename I>
	unordered_map<string, std::vector<Ind_t>> getElites(size_t n,
	                                                    const std::vector<I> &popVec) {
		std::vector<string> obj;
		for (auto &o : ref(popVec[0]).fitnesses) obj.push_back(o.first);
		return getElites(obj, n, popVec);
	}
	unordered_map<string, std::vector<Ind_t>> getLastGenElites(size_t n) {
		std::vector<string> obj;
		for (auto &o : population[0].fitnesses) obj.push_back(o.first);
		return getElites(obj, n, previousGenerations.back());
	}
	template <typename I>
	unordered_map<string, std::vector<Ind_t>> getElites(const std::vector<string> &obj,
	                                                    size_t n,
	                                                    const std::vector<I> &popVec) {
		if (verbosity >= 3) {
			cerr << "getElites : nbObj = " << obj.size() << " n = " << n << endl;
		}
		unordered_map<string, std::vector<Ind_t>> elites;
		for (auto &o : obj) {
			elites[o] = std::vector<Ind_t>();
			elites[o].push_back(ref(popVec[0]));
			size_t worst = 0;
			for (size_t i = 1; i < n && i < popVec.size(); ++i) {
				elites[o].push_back(ref(popVec[i]));
				if (isBetter(elites[o][worst].fitnesses.at(o), ref(popVec[i]).fitnesses.at(o)))
					worst = i;
			}
			for (size_t i = n; i < popVec.size(); ++i) {
				if (isBetter(ref(popVec[i]).fitnesses.at(o), elites[o][worst].fitnesses.at(o))) {
					elites[o][worst] = ref(popVec[i]);
					for (size_t j = 0; j < n; ++j) {
						if (isBetter(elites[o][worst].fitnesses.at(o), elites[o][j].fitnesses.at(o)))
							worst = j;
					}
				}
			}
		}
		return elites;
	}

	/*********************************************************************************
	 *                          NOVELTY RELATED METHODS
	 ********************************************************************************/
	// Novelty works with footprints. A footprint is just a std::vector of std::vector of
	// doubles. It is recommended that those doubles are within a same order of magnitude.
	// Each std::vector<double> is a "snapshot": it represents the state of the evaluation
	// of one individual at a certain time. Thus, a complete footprint is a combination of
	// one or more snapshot taken at different points in the simulation (a
	// std::vector<vector<double>>). Snapshot must be of same size accross individuals.
	// Footprint must be set in the evaluator (see examples)
	// Options for novelty:
	//  - Local Competition:
	//

	distanceMatrix_t defaultComputeDistanceMatrix(const std::vector<Ind_t> &ar) {
		// this computes both dist(i,j) and dist(j,i), so they can be different.
		distanceMatrix_t dmat(ar.size(), std::vector<double>(ar.size()));
		for (size_t i = 0; i < ar.size(); ++i) {
			for (size_t j = 0; j < ar.size(); ++j) {
				if (i != j)
					dmat[i][j] = computeFootprintDistance(ar[i].footprint, ar[j].footprint);
			}
		}
		return dmat;
	}

	std::vector<size_t> findKNN(size_t i, size_t K, const distanceMatrix_t &dmat) {
		// returns the K nearest neighbors of i, according to the distance matrix dmat
		if (dmat.size() == 0) return std::vector<size_t>();
		assert(dmat[i].size() == dmat.size());
		assert(i < dmat.size());
		const std::vector<double> &distances = dmat[i];
		std::vector<size_t> indices(distances.size());
		std::iota(indices.begin(), indices.end(), 0);

		size_t k = std::max(std::min(K, distances.size() - 1), (size_t)0u);

		std::nth_element(
		    indices.begin(), indices.begin() + k, indices.end(),
		    [&distances](size_t a, size_t b) { return distances[a] < distances[b]; });

		indices.erase(std::remove(indices.begin(), indices.end(), i),
		              indices.end());  // remove itself from the knn list
		indices.resize(k);
		return indices;
	}

	void updateNovelty() {
		// we append the current population to the archive
		auto savedArchiveSize = archive.size();
		for (auto &ind : population) archive.push_back(ind);

		// we compute the distance matrix.
		// distanceMatrix[i][j] == distanceMatrix[j][i] == distance(archive[i], archive[j])
		std::vector<std::vector<double>> distanceMatrix = computeDistanceMatrix(archive);

		// then update the novelty field of every member of the population
		for (size_t p_i = 0; p_i < population.size(); p_i++) {
			size_t i = savedArchiveSize + p_i;  // individuals'id in the archive
			assert(population[p_i].id == archive[i].id);

			std::vector<size_t> knn = findKNN(i, KNN, distanceMatrix);
			if (nslc) {
				// local competition is enabled

				// we put all objectives other than novelty into objs
				auto objs = getAllObjectives(population[p_i]);
				if (objs.count("novelty")) objs.erase("novelty");
				if (objs.count("local_score")) objs.erase("local_score");

				std::vector<Ind_t *> knnPtr;  // pointers to knn individuals
				for (auto k : knn) knnPtr.push_back(&archive[k]);
				knnPtr.push_back(&archive[i]);  // + itself

				// we normalize the rank
				double knnSize = knn.size() > 0 ? static_cast<double>(knn.size()) : 1.0;
				double localScore =
				    static_cast<double>(getParetoRank(knnPtr, knnPtr.size(), objs)) / knnSize;
				population[p_i].fitnesses["local_score"] = 1.0 - localScore;
			}

			// sum = sum of distances between i and its knn
			// novelty = avg dist to knn
			double sum = 0;
			for (auto &j : knn) sum += distanceMatrix[i][j];
			population[p_i].fitnesses["novelty"] = sum / (double)knn.size();

			printLn(2, "Novelty for ind ", population[p_i].id, " -> ",
			        population[p_i].fitnesses["novelty"]);
			printLn(3, "Ind ", population[p_i].id, " footprint is ",
			        footprintToString(population[p_i].footprint));
		}

		// now we add random individuals to the archive
		std::vector<Ind_t> toBeAdded;
		std::uniform_int_distribution<size_t> d(0, population.size() - 1);
		for (size_t i = 0; i < nbOfArchiveAdditionsPerGeneration; ++i)
			toBeAdded.push_back(population[d(globalRand)]);

		// first we erase the entire pop that we had appended to the archive
		archive.erase(archive.begin() + static_cast<long>(savedArchiveSize), archive.end());
		// then we add the newly selected individuals
		archive.insert(std::end(archive), std::begin(toBeAdded), std::end(toBeAdded));
		printLn(2, "Added ", toBeAdded.size(), " new individuals to the archive.");
		printLn(2, "New archive size = ", archive.size());

		// TODO: archive maintenance: allow to recompute every novelty score
		// + maintain a certain size
	}

	template <typename T> static inline std::string footprintToString(const T &f) {
		std::ostringstream res;
		res << "👣  " << json(f).dump();
		return res.str();
	}

	string selectMethodToString(const SelectionMethod &sm) {
		switch (sm) {
			case SelectionMethod::paretoTournament:
				return "pareto tournament";
			case SelectionMethod::randomObjTournament:
				return "random objective tournament";
		}
		return "???";
	}

	/*********************************************************************************
	 *                           STATS, LOGS & PRINTING
	 ********************************************************************************/
	void printStart() {
		int nbCol = 55;
		std::cout << std::endl << GAGA_COLOR_GREY;
		for (int i = 0; i < nbCol - 1; ++i) std::cout << "━";
		std::cout << std::endl;
		std::cout << GAGA_COLOR_YELLOW << "              ☀     " << GAGA_COLOR_NORMAL
		          << " Starting GAGA " << GAGA_COLOR_YELLOW << "    ☀ " << GAGA_COLOR_NORMAL;
		std::cout << std::endl;
		std::cout << GAGA_COLOR_BLUE << "                      ¯\\_ಠ ᴥ ಠ_/¯" << std::endl
		          << GAGA_COLOR_GREY;
		for (int i = 0; i < nbCol - 1; ++i) std::cout << "┄";
		std::cout << std::endl << GAGA_COLOR_NORMAL;
		std::cout << "  ▹ population size = " << GAGA_COLOR_BLUE << popSize
		          << GAGA_COLOR_NORMAL << std::endl;
		std::cout << "  ▹ nb of elites = " << GAGA_COLOR_BLUE << nbElites << GAGA_COLOR_NORMAL
		          << std::endl;
		std::cout << "  ▹ nb of tournament competitors = " << GAGA_COLOR_BLUE
		          << tournamentSize << GAGA_COLOR_NORMAL << std::endl;
		std::cout << "  ▹ selection = " << GAGA_COLOR_BLUE
		          << selectMethodToString(selecMethod) << GAGA_COLOR_NORMAL << std::endl;
		std::cout << "  ▹ mutation rate = " << GAGA_COLOR_BLUE << mutationRate
		          << GAGA_COLOR_NORMAL << std::endl;
		std::cout << "  ▹ crossover rate = " << GAGA_COLOR_BLUE << crossoverRate
		          << GAGA_COLOR_NORMAL << std::endl;
		std::cout << "  ▹ writing results in " << GAGA_COLOR_BLUE << folder
		          << GAGA_COLOR_NORMAL << std::endl;
		if (novelty) {
			std::cout << "  ▹ novelty is " << GAGA_COLOR_GREEN << "enabled" << GAGA_COLOR_NORMAL
			          << std::endl;
			std::cout << "    - KNN size = " << GAGA_COLOR_BLUE << KNN << GAGA_COLOR_NORMAL
			          << std::endl;
		} else {
			std::cout << "  ▹ novelty is " << GAGA_COLOR_RED << "disabled" << GAGA_COLOR_NORMAL
			          << std::endl;
		}
		if (speciation) {
			std::cout << "  ▹ speciation is " << GAGA_COLOR_GREEN << "enabled"
			          << GAGA_COLOR_NORMAL << std::endl;
			std::cout << "    - minSpecieSize size = " << GAGA_COLOR_BLUE << minSpecieSize
			          << GAGA_COLOR_NORMAL << std::endl;
			std::cout << "    - speciationThreshold = " << GAGA_COLOR_BLUE
			          << speciationThreshold << GAGA_COLOR_NORMAL << std::endl;
			std::cout << "    - speciationThresholdIncrement = " << GAGA_COLOR_BLUE
			          << speciationThresholdIncrement << GAGA_COLOR_NORMAL << std::endl;
			std::cout << "    - minSpeciationThreshold = " << GAGA_COLOR_BLUE
			          << minSpeciationThreshold << GAGA_COLOR_NORMAL << std::endl;
			std::cout << "    - maxSpeciationThreshold = " << GAGA_COLOR_BLUE
			          << maxSpeciationThreshold << GAGA_COLOR_NORMAL << std::endl;
		} else {
			std::cout << "  ▹ speciation is " << GAGA_COLOR_RED << "disabled"
			          << GAGA_COLOR_NORMAL << std::endl;
		}
		std::cout << GAGA_COLOR_GREY;
		for (int i = 0; i < nbCol - 1; ++i) std::cout << "━";
		std::cout << GAGA_COLOR_NORMAL << std::endl;
	}
	void updateStats(double totalTime) {
		// stats organisations :
		// "global" -> {"genTotalTime", "indTotalTime", "maxTime", "nEvals", "nObjs"}
		// "obj_i" -> {"avg", "worst", "best"}
		assert(previousGenerations.size());
		auto &lastGen = previousGenerations.back();
		std::map<std::string, std::map<std::string, double>> currentGenStats;
		currentGenStats["global"]["genTotalTime"] = totalTime;
		double indTotalTime = 0.0, maxTime = 0.0;
		int nEvals = 0;
		int nObjs = static_cast<int>(lastGen[0].fitnesses.size());
		for (const auto &o : lastGen[0].fitnesses) {
			currentGenStats[o.first] = {
			    {{"avg", 0.0}, {"worst", o.second}, {"best", o.second}}};
		}
		// computing min, avg, max from custom individual stats
		auto customStatsNames = lastGen[0].stats;
		std::map<string, std::tuple<double, double, double>> customStats;
		for (auto &csn : customStatsNames)
			customStats[csn.first] = std::make_tuple<double, double, double>(
			    std::numeric_limits<double>::max(), 0.0, std::numeric_limits<double>::min());
		for (auto &ind : lastGen) {
			for (auto &cs : customStats) {
				double v = ind.stats.at(cs.first);
				if (v < std::get<0>(cs.second)) std::get<0>(cs.second) = v;
				if (v > std::get<2>(cs.second)) std::get<2>(cs.second) = v;
				std::get<1>(cs.second) += v / static_cast<double>(lastGen.size());
			}
		}
		for (auto &cs : customStats) {
			{
				std::ostringstream n;
				n << cs.first << "_min";
				currentGenStats["custom"][n.str()] = std::get<0>(cs.second);
			}
			{
				std::ostringstream n;
				n << cs.first << "_avg";
				currentGenStats["custom"][n.str()] = std::get<1>(cs.second);
			}
			{
				std::ostringstream n;
				n << cs.first << "_max";
				currentGenStats["custom"][n.str()] = std::get<2>(cs.second);
			}
		}
		for (const auto &ind : lastGen) {
			indTotalTime += ind.evalTime;
			for (const auto &o : ind.fitnesses) {
				currentGenStats[o.first].at("avg") +=
				    (o.second / static_cast<double>(lastGen.size()));
				if (isBetter(o.second, currentGenStats[o.first].at("best")))
					currentGenStats[o.first].at("best") = o.second;
				if (!isBetter(o.second, currentGenStats[o.first].at("worst")))
					currentGenStats[o.first].at("worst") = o.second;
			}
			if (ind.evalTime > maxTime) maxTime = ind.evalTime;
			if (!ind.wasAlreadyEvaluated) ++nEvals;
		}
		currentGenStats["global"]["indTotalTime"] = indTotalTime;
		currentGenStats["global"]["maxTime"] = maxTime;
		currentGenStats["global"]["nEvals"] = nEvals;
		currentGenStats["global"]["nObjs"] = nObjs;
		if (speciation) {
			currentGenStats["global"]["nSpecies"] = species.size();
		}
		genStats.push_back(currentGenStats);
	}

 public:
	void printGenStats(size_t n) {
		const size_t l = 80;
		std::cout << tableHeader(l);
		std::ostringstream output;
		const auto &globalStats = genStats[n].at("global");
		output << "Generation " << GAGA_COLOR_CYANBOLD << n << GAGA_COLOR_NORMAL
		       << " ended in " << GAGA_COLOR_BLUE << globalStats.at("genTotalTime")
		       << GAGA_COLOR_NORMAL << "s";
		std::cout << tableCenteredText(
		    l, output.str(),
		    GAGA_COLOR_BLUEBOLD GAGA_COLOR_NORMAL GAGA_COLOR_BLUE GAGA_COLOR_NORMAL);
		output = std::ostringstream();
		output << GAGA_COLOR_GREYBOLD << "(" << globalStats.at("nEvals") << " evaluations, "
		       << globalStats.at("nObjs") << " objs";
		if (speciation) output << ", " << species.size() << " species";
		output << ")" << GAGA_COLOR_NORMAL;
		std::cout << tableCenteredText(l, output.str(),
		                               GAGA_COLOR_GREYBOLD GAGA_COLOR_NORMAL);
		std::cout << tableSeparation(l);
		double timeRatio = 0;
		if (globalStats.at("genTotalTime") > 0)
			timeRatio = globalStats.at("indTotalTime") / globalStats.at("genTotalTime");
		output = std::ostringstream();
		output << "🕝  max: " << GAGA_COLOR_BLUE << globalStats.at("maxTime")
		       << GAGA_COLOR_NORMAL << "s";
		output << ", 🕝  sum: " << GAGA_COLOR_BLUEBOLD << globalStats.at("indTotalTime")
		       << GAGA_COLOR_NORMAL << "s (x" << timeRatio << " ratio)";
		std::cout << tableCenteredText(
		    l, output.str(),
		    GAGA_COLOR_CYANBOLD GAGA_COLOR_NORMAL GAGA_COLOR_BLUE GAGA_COLOR_NORMAL "      ");
		std::cout << tableSeparation(l);
		for (const auto &o : genStats[n]) {
			if (o.first != "global" && o.first != "custom") {
				output = std::ostringstream();
				output << GAGA_COLOR_GREYBOLD << "--◇" << GAGA_COLOR_GREENBOLD << std::setw(10)
				       << o.first << GAGA_COLOR_GREYBOLD << " ❯ " << GAGA_COLOR_NORMAL
				       << " worst: " << GAGA_COLOR_YELLOW << std::setw(12) << o.second.at("worst")
				       << GAGA_COLOR_NORMAL << ", avg: " << GAGA_COLOR_YELLOWBOLD << std::setw(12)
				       << o.second.at("avg") << GAGA_COLOR_NORMAL
				       << ", best: " << GAGA_COLOR_REDBOLD << std::setw(12) << o.second.at("best")
				       << GAGA_COLOR_NORMAL;
				std::cout << tableText(
				    l, output.str(),
				    "    " GAGA_COLOR_GREYBOLD GAGA_COLOR_GREENBOLD GAGA_COLOR_GREYBOLD
				        GAGA_COLOR_NORMAL GAGA_COLOR_YELLOWBOLD GAGA_COLOR_NORMAL
				            GAGA_COLOR_YELLOW GAGA_COLOR_NORMAL GAGA_COLOR_GREENBOLD
				                GAGA_COLOR_NORMAL);
			}
		}
		if (genStats[n].count("custom")) {
			std::cout << tableSeparation(l);
			for (const auto &o : genStats[n]["custom"]) {
				output = std::ostringstream();
				output << GAGA_COLOR_GREENBOLD << std::setw(15) << o.first << GAGA_COLOR_GREYBOLD
				       << " ❯ " << GAGA_COLOR_NORMAL << std::setw(15) << o.second;
				std::cout << tableCenteredText(
				    l, output.str(), GAGA_COLOR_GREENBOLD GAGA_COLOR_GREYBOLD GAGA_COLOR_NORMAL);
			}
		}
		std::cout << tableFooter(l);
	}

	void printIndividualStats(const Ind_t &ind) {
		std::ostringstream output;
		output << GAGA_COLOR_GREYBOLD << "[" << GAGA_COLOR_YELLOW << procId
		       << GAGA_COLOR_GREYBOLD << "]-▶ " << GAGA_COLOR_NORMAL;
		for (const auto &o : ind.fitnesses)
			output << " " << o.first << ": " << GAGA_COLOR_BLUEBOLD << std::setw(12) << o.second
			       << GAGA_COLOR_NORMAL << GAGA_COLOR_GREYBOLD << " |" << GAGA_COLOR_NORMAL;
		output << " 🕝 : " << GAGA_COLOR_BLUE << ind.evalTime << "s" << GAGA_COLOR_NORMAL;
		for (const auto &o : ind.stats) output << " ; " << o.first << ": " << o.second;
		if (ind.wasAlreadyEvaluated)
			output << GAGA_COLOR_GREYBOLD << " | (already evaluated)\n" << GAGA_COLOR_NORMAL;
		else
			output << "\n";
		if ((!novelty && verbosity >= 2) || verbosity >= 3) output << ind.infos << std::endl;
		if (novelty && verbosity >= 2)
			output << footprintToString(ind.footprint) << std::endl;
		std::cout << output.str();
	}

	GAGA_PROTECTED_TESTABLE :

	    std::string
	    tableHeader(unsigned int l) {
		std::ostringstream output;
		output << "┌";
		for (auto i = 0u; i < l; ++i) output << "─";
		output << "┓\n";
		return output.str();
	}

	std::string tableFooter(unsigned int l) {
		std::ostringstream output;
		output << "┗";
		for (auto i = 0u; i < l; ++i) output << "─";
		output << "┛\n";
		return output.str();
	}

	std::string tableSeparation(unsigned int l) {
		std::ostringstream output;
		output << "|" << GAGA_COLOR_GREYBOLD;
		for (auto i = 0u; i < l; ++i) output << "-";
		output << GAGA_COLOR_NORMAL << "|\n";
		return output.str();
	}

	std::string tableText(int l, std::string txt, std::string unprinted = "") {
		std::ostringstream output;
		int txtsize = static_cast<int>(txt.size() - unprinted.size());
		output << "|" << txt;
		for (auto i = txtsize; i < l; ++i) output << " ";
		output << "|\n";
		return output.str();
	}

	std::string tableCenteredText(int l, std::string txt, std::string unprinted = "") {
		std::ostringstream output;
		int txtsize = static_cast<int>(txt.size() - unprinted.size());
		int space = l - txtsize;
		output << "|";
		for (int i = 0; i < space / 2; ++i) output << " ";
		output << txt;
		for (int i = (space / 2) + txtsize; i < l; ++i) output << " ";
		output << "|\n";
		return output.str();
	}

 public:
	/*********************************************************************************
	 *                         SAVING STUFF
	 ********************************************************************************/
	void saveBests(size_t n) {
		if (n > 0) {
			const std::vector<Ind_t> &p = previousGenerations.back();
			// save n bests dnas for all objectives
			std::vector<string> objectives;
			for (auto &o : p[0].fitnesses) {
				objectives.push_back(o.first);  // we need to know objective functions
			}
			auto elites = getElites(objectives, n, p);
			std::stringstream baseName;
			baseName << folder << "/gen" << currentGeneration;
			mkd(baseName.str().c_str());
			if (verbosity >= 3) {
				cerr << "created directory " << baseName.str() << endl;
			}
			for (auto &e : elites) {
				int id = 0;
				for (auto &i : e.second) {
					std::stringstream fileName;
					fileName << baseName.str() << "/" << e.first << "_" << i.fitnesses.at(e.first)
					         << "_" << id++ << ".dna";
					std::ofstream fs(fileName.str());
					if (!fs) {
						cerr << "Cannot open the output file." << endl;
					}
					fs << i.dna.serialize();
					fs.close();
				}
			}
		}
	}

	void saveParetoFront() {
		std::vector<Ind_t> &p = previousGenerations.back();
		std::vector<Ind_t *> pop;
		for (size_t i = 0; i < p.size(); ++i) {
			pop.push_back(&p[i]);
		}

		auto pfront = getParetoFront(pop);
		std::stringstream baseName;
		baseName << folder << "/gen" << currentGeneration;
		mkd(baseName.str().c_str());
		if (verbosity >= 3) {
			std::cout << "created directory " << baseName.str() << std::endl;
		}

		int id = 0;
		for (const auto &ind : pfront) {
			std::stringstream filename;
			filename << baseName.str() << "/";
			for (const auto &f : ind->fitnesses) {
				filename << f.first << f.second << "_";
			}
			filename << id++ << ".dna";

			std::ofstream fs(filename.str());
			if (!fs) {
				std::cerr << "Cannot open the output file.\n";
			}
			fs << ind->dna.serialize();
			fs.close();
		}
	}

	void saveGenStats() {
		std::stringstream csv;
		std::stringstream fileName;
		fileName << folder << "/gen_stats.csv";
		csv << "generation";
		if (genStats.size() > 0) {
			for (const auto &cat : genStats[0]) {
				std::stringstream column;
				column << cat.first << "_";
				for (const auto &s : cat.second) {
					csv << "," << column.str() << s.first;
				}
			}
			csv << endl;
			for (size_t i = 0; i < genStats.size(); ++i) {
				csv << i;
				for (const auto &cat : genStats[i]) {
					for (const auto &s : cat.second) {
						csv << "," << s.second;
					}
				}
				csv << endl;
			}
		}
		std::ofstream fs(fileName.str());
		if (!fs) cerr << "Cannot open the output file." << endl;
		fs << csv.str();
		fs.close();
	}

	// gen, idInd, fit0, fit1, time
	void saveIndStats() {
		std::stringstream csv;
		std::stringstream fileName;
		fileName << folder << "/ind_stats.csv";
		static bool indStatsWritten = false;
		auto &lastGen = previousGenerations.back();
		if (!indStatsWritten) {
			csv << "generation,idInd,";
			for (auto &o : lastGen[0].fitnesses) csv << o.first << ",";
			for (auto &o : lastGen[0].stats) csv << o.first << ",";
			csv << "time" << std::endl;
			indStatsWritten = true;
		}
		for (size_t i = 0; i < lastGen.size(); ++i) {
			const auto &p = lastGen[i];
			csv << currentGeneration << "," << i << ",";
			for (auto &o : p.fitnesses) csv << o.second << ",";
			for (auto &o : p.stats) csv << o.second << ",";
			csv << p.evalTime << std::endl;
		}
		std::ofstream fs;
		fs.open(fileName.str(), std::fstream::out | std::fstream::app);
		if (!fs) cerr << "Cannot open the output file." << endl;
		fs << csv.str();
		fs.close();
	}

	void saveIndStats_OneLinePerGen() {
		std::stringstream csv;
		std::stringstream fileName;
		fileName << folder << "/ind_stats.csv";

		static bool has_been_written = false;

		if (!has_been_written) {
			csv << "generation";
			size_t i = 0;
			for (const auto &ind : previousGenerations.back()) {
				csv << ",ind" << i++;
				for (const auto &f : ind.fitnesses) {
					csv << "," << f.first;
				}
				csv << ",is_on_pareto_front,eval_time";
			}
			csv << endl;

			has_been_written = true;
		}

		std::vector<int> is_on_front(previousGenerations.back().size(), false);

		if (selecMethod == SelectionMethod::paretoTournament) {
			std::vector<Ind_t *> pop;

			for (auto &p : previousGenerations.back()) {
				pop.push_back(&p);
			}

			auto front = getParetoFront(pop);

			for (size_t i = 0; i < pop.size(); ++i) {
				Ind_t *ind0 = pop[i];
				int found = 0;

				for (size_t j = 0; !found && (j < front.size()); ++j) {
					Ind_t *ind1 = front[j];

					if (ind1 == ind0) {
						found = 1;
					}
				}

				is_on_front[i] = found;
			}
		}

		{
			csv << currentGeneration;
			size_t ind_id = 0;
			for (const auto &ind : previousGenerations.back()) {
				csv << "," << ind_id;
				for (const auto &f : ind.fitnesses) {
					csv << "," << f.second;
				}

				csv << "," << is_on_front[ind_id];
				csv << "," << ind.evalTime;
				++ind_id;
			}
			csv << endl;
		}

		std::ofstream fs;
		fs.open(fileName.str(), std::fstream::out | std::fstream::app);
		if (!fs) {
			cerr << "Cannot open the output file." << endl;
		}
		fs << csv.str();
		fs.close();
	}

	GAGA_PROTECTED_TESTABLE :

	    int
	    mkd(const char *p) {
#ifdef _WIN32
		return _mkdir(p);
#else
		return mkdir(p, 0777);
#endif
	}

	void mkPath(char *file_path) {
		assert(file_path && *file_path);
		char *p;
		for (p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
			*p = '\0';
			if (mkd(file_path) == -1) {
				if (errno != EEXIST) {
					*p = '/';
					return;
				}
			}
			*p = '/';
		}
	}

	void createFolder(string baseFolder) {
		if (baseFolder.back() != '/') baseFolder += "/";
		struct stat sb;
		char bFChar[500];
		strncpy(bFChar, baseFolder.c_str(), 500);
		mkPath(bFChar);
		auto now = system_clock::now();
		time_t now_c = system_clock::to_time_t(now);
		struct tm *parts = localtime(&now_c);

		std::stringstream fname;
		fname << evaluatorName << "_" << parts->tm_mday << "_" << parts->tm_mon + 1 << "_";
		int cpt = 0;
		std::stringstream ftot;
		do {
			ftot.clear();
			ftot.str("");
			ftot << baseFolder << fname.str() << cpt;
			cpt++;
		} while (stat(ftot.str().c_str(), &sb) == 0);  // && S_ISDIR(sb.st_mode));
		folder = ftot.str();
		mkd(folder.c_str());
	}

 public:
	void loadPop(string file) {
		std::ifstream t(file);
		std::stringstream buffer;
		buffer << t.rdbuf();
		auto o = json::parse(buffer.str());
		assert(o.count("population"));
		if (o.count("generation")) {
			currentGeneration = o.at("generation");
		} else {
			currentGeneration = 0;
		}
		population.clear();
		for (auto ind : o.at("population")) {
			population.push_back(Ind_t(DNA(ind.at("dna"))));
			population[population.size() - 1].evaluated = false;
		}
	}

	void savePop() {
		json o = Ind_t::popToJSON(previousGenerations.back());
		o["evaluator"] = evaluatorName;
		o["generation"] = currentGeneration;
		std::stringstream baseName;
		baseName << folder << "/gen" << currentGeneration;
		mkd(baseName.str().c_str());
		std::stringstream fileName;
		fileName << baseName.str() << "/pop" << currentGeneration << ".pop";
		std::ofstream file;
		file.open(fileName.str());
		file << o.dump();
		file.close();
	}

	void saveArchive() {
		json o = Ind_t::popToJSON(archive);
		o["evaluator"] = evaluatorName;
		std::stringstream baseName;
		baseName << folder << "/gen" << currentGeneration;
		mkd(baseName.str().c_str());
		std::stringstream fileName;
		fileName << baseName.str() << "/archive" << currentGeneration << ".pop";
		std::ofstream file;
		file.open(fileName.str());
		file << o.dump();
		file.close();
	}
};
}  // namespace GAGA
#endif
