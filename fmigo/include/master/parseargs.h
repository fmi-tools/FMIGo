#ifndef PARSEARGS_H
#define PARSEARGS_H

#include <fmilib.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include "common/CSV-parser.h"

namespace fmitcp_master {

struct connection {
    connection() {
        slope = 1;
        intercept = 0;
        needs_type = true;
    }
    bool needs_type;
    fmi2_base_type_enu_t fromType;
    int fromFMU;                // Index of FMU
    int fromOutputVR;           // Value reference
    fmi2_base_type_enu_t toType;
    int toFMU;                  // FMU index
    int toInputVR;              // Value reference
    double slope, intercept;    // for unit conversion. y = slope*x + intercept
    std::string fromOutputVRorNAME;           // Value reference
    std::string toInputVRorNAME;              // Value reference
};

struct strongconnection {
    std::string type;
    int fromFMU;
    int toFMU;
    std::vector<std::string> vrORname;              // Value reference
};

struct param {
    int valueReference;
    fmi2_base_type_enu_t type;
    std::string stringValue;            // String version, always set to what the user wrote
    int intValue;                       // Integer
    double realValue;                   // Real
    int boolValue;                      // Boolean
};

 typedef std::map<std::pair<int,fmi2_base_type_enu_t>, std::vector<param> > param_map;

enum FILEFORMAT {
    csv,
    tikz,
    none,   // special output format meaning "don't printf() anything"
};

enum METHOD {
    jacobi,
    gs,
    me
};

fmi2_base_type_enu_t type_from_char(std::string type);

/**
 * @brief Parses the command line arguments and stores in the given variable pointer targets.
 * @param argc Given by system
 * @param argv Given by system
 * @return int Returns 0 if the program should proceed, 1 if the program should end.
 */
void parseArguments( int argc,
                    char *argv[],
                    std::vector<std::string> *fmuFilePaths,
                    std::vector<connection> *connections,
                    std::vector<std::deque<std::string> > *params,
                    double* tEnd,
                    double* timeStepSize,
                    jm_log_level_enu_t *loglevel,
                    char* csv_separator,
                    std::string *outFilePath,
                    enum FILEFORMAT * fileFormat,
                    enum METHOD * method,
                    int * realtimeMode,
                    std::vector<int> *stepOrder,
                    std::vector<int> *fmuVisibilities,
                    std::vector<strongconnection> *strongConnections,
                    std::string *hdf5Filename,
                    std::string *fieldnameFilename,
                    bool *holonomic,
                    double *compliance,
                    int *command_port,
                    int *results_port,
                    bool *paused,
                    bool *solveLoops,
                    bool *useHeadersInCSV,
                    fmigo_csv_fmu *csv_fmu
                    );
}

#endif /* PARSEARGS_H */
