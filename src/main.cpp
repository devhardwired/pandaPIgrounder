#include <fstream>
#include <iostream>
#include <map>
#include <memory>

#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <getopt.h>

#include "debug.h"
#include "grounding.h"
#include "hierarchy-typing.h"
#include "model.h"
#include "parser.h"
#include "planning-graph.h"
#include "sasinvariants.h"

int main (int argc, char * argv[])
{
	struct option options[] = {
		{"output-domain",      	                            no_argument,    NULL,   'O'},
		{"primitive",          	                            no_argument,    NULL,   'P'},
		{"debug",              	                            no_argument,    NULL,   'd'},
		{"print-domain",       	                            no_argument,    NULL,   'p'},
		{"quiet",              	                            no_argument,    NULL,   'q'},
		{"print-timings",     	                            no_argument,    NULL,   't'},
		{"invariants",         	                            no_argument,    NULL,   'i'},
		{"only-ground",         	                        no_argument,    NULL,   'g'},
		{"h2", 			        	                        no_argument,    NULL,   '2'},
		
		{"no-hierarchy-typing",	                            no_argument,    NULL,   'h'},
		{"no-literal-pruning", 	                            no_argument,    NULL,   'l'},
		{"no-abstract-expansion",                           no_argument,    NULL,   'e'},
		{"no-method-precondition-pruning",                  no_argument,    NULL,   'm'},
		{"future-caching-by-initially-matched-precondition",no_argument,    NULL,   'f'},

		
		{NULL,                            0,              NULL,   0},
	};

	bool primitiveMode = false;
	bool quietMode = false;
	bool debugMode = false;
	bool computeInvariants = false;
	bool outputForPlanner = true; // don't output in 
	bool optionsValid = true;
	bool outputDomain = false;
	

	bool enableHierarchyTyping = true;
	bool removeUselessPredicates = true;
	bool expandChoicelessAbstractTasks = true;
	bool pruneEmptyMethodPreconditions = true;
	bool futureCachingByPrecondition = false;
	bool h2mutexes = false;
	bool printTimings = false;
	while (true)
	{
		int c = getopt_long_only (argc, argv, "dpqiOPhlemgft2", options, NULL);
		if (c == -1)
			break;
		if (c == '?' || c == ':')
		{
			// Invalid option; getopt_long () will print an error message
			optionsValid = false;
			continue;
		}

		if (c == 'P')
			primitiveMode = true;
		else if (c == 'd')
			debugMode = true;
		else if (c == 'p')
			outputDomain = true;
		else if (c == 'q')
			quietMode = true;
		else if (c == 'i')
			computeInvariants = true;
		else if (c == 'O')
			outputDomain = true;
		
		else if (c == 'h')
			enableHierarchyTyping = false;
		else if (c == 'l')
			removeUselessPredicates = false;
		else if (c == 'e')
			expandChoicelessAbstractTasks = false;
		else if (c == 'm')
			pruneEmptyMethodPreconditions = false;
		else if (c == 'g')
			outputForPlanner = false;
		else if (c == 'f')
			futureCachingByPrecondition = true;
		else if (c == 't')
			printTimings = false;
		else if (c == '2')
			h2mutexes = true;
	}
	
	if (!optionsValid)
	{
		std::cout << "Invalid options. Exiting." << std::endl;
		return 1;
	}

	if (debugMode)
		setDebugMode (debugMode);

	if (primitiveMode && !quietMode)
		std::cerr << "Note: Running in benchmark mode; grounding results will not be printed." << std::endl;

	std::vector<std::string> inputFiles;
	for (int i = optind; i < argc; ++i)
	{
		inputFiles.push_back (argv[i]);
	}

	std::string inputFilename = "-";
	std::string outputFilename = "-";

	if (inputFiles.size() > 2){
		std::cerr << "You may specify at most two files as parameters: the input and the output file" << std::endl;
		return 1;
	} else {
		if (inputFiles.size())
			inputFilename = inputFiles[0];
		if (inputFiles.size() > 1)
			outputFilename = inputFiles[1];
	}

	std::istream * inputStream;
	if (inputFilename == "-")
	{
		if (!quietMode)
			std::cerr << "Reading input from standard input." << std::endl;

		inputStream = &std::cin;
	}
	else
	{
		if (!quietMode)
			std::cerr << "Reading input from " << inputFilename << "." << std::endl;

		std::ifstream * fileInput  = new std::ifstream(inputFilename);
		if (!fileInput->good())
		{
			std::cerr << "Unable to open input file " << inputFilename << ": " << strerror (errno) << std::endl;
			return 1;
		}

		inputStream = fileInput;
	}


	Domain domain;
	Problem problem;
	bool success = readInput (*inputStream, domain, problem);

	std::ostream * outputStream;
	if (outputFilename == "-")
	{
		if (!quietMode)
			std::cerr << "Writing output to standard output." << std::endl;

		outputStream = &std::cout;
	}
	else
	{
		if (!quietMode)
			std::cerr << "Writing output to " << outputFilename << "." << std::endl;

		std::ofstream * fileOutput  = new std::ofstream(outputFilename);

		if (!fileOutput->good ())
		{
			std::cerr << "Unable to open output file " << outputFilename << ": " << strerror (errno) << std::endl;
			return 1;
		}

		outputStream = fileOutput;
	}

	if (!success)
	{
		std::cerr << "Failed to read input data!" << std::endl;
		return 1;
	}
	if (!quietMode)
		std::cerr << "Parsing done." << std::endl;

	if (outputDomain)
	{
		printDomainAndProblem (domain, problem);
		return 1;
	}


	std::vector<invariant> invariants;
	if (computeInvariants)
	{
		invariants = computeSASPlusInvariants(domain, problem);
	}

	// Run the actual grounding procedure
	if (primitiveMode)
	{
		// Just run the PG - this is for speed testing
		std::unique_ptr<HierarchyTyping> hierarchyTyping;
		if (enableHierarchyTyping)
			hierarchyTyping = std::make_unique<HierarchyTyping> (domain, problem);

		/*GpgPlanningGraph pg (domain, problem);
		std::vector<GroundedTask> groundedTasks;
		std::set<Fact> reachableFacts;
		runGpg (pg, groundedTasks, reachableFacts, hierarchyTyping.get (), quietMode);*/
	}
	else
	{
		run_grounding (domain, problem, *outputStream, 
				enableHierarchyTyping, removeUselessPredicates, expandChoicelessAbstractTasks, pruneEmptyMethodPreconditions, 
				futureCachingByPrecondition, 
				h2mutexes, 
				outputForPlanner , printTimings, quietMode);
	}

}
