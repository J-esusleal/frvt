/**
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include <fstream>
#include <iostream>
#include <cstring>
#include <iterator>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>

#include "frvt_morph.h"
#include "util.h"

using namespace std;
using namespace FRVT;
using namespace FRVT_MORPH;

std::map<Action, FRVT_MORPH::ImageLabel> mapActionToMorphLabel =
{
    { Action::DetectNonScannedMorph, FRVT_MORPH::ImageLabel::NonScanned },
    { Action::DetectScannedMorph, FRVT_MORPH::ImageLabel::Scanned },
    { Action::DetectUnknownMorph, FRVT_MORPH::ImageLabel::Unknown },
    { Action::DetectNonScannedMorphWithProbeImg, FRVT_MORPH::ImageLabel::NonScanned },
    { Action::DetectScannedMorphWithProbeImg, FRVT_MORPH::ImageLabel::Scanned },
    { Action::DetectUnknownMorphWithProbeImg, FRVT_MORPH::ImageLabel::Unknown },
    { Action::DetectNonScannedMorphWithProbeImgAndMeta, FRVT_MORPH::ImageLabel::NonScanned },
    { Action::DetectScannedMorphWithProbeImgAndMeta, FRVT_MORPH::ImageLabel::Scanned },
    { Action::DetectUnknownMorphWithProbeImgAndMeta, FRVT_MORPH::ImageLabel::Unknown }
};

void
writeImagePPM(
    const FRVT::Image &image,
    const std::string &outputStem)
{
    if (image.width == 0 || image.height == 0) {
        cerr << "Failed to write invalid image: " << outputStem << ".  " 
            << "Invalid image dimensions." << endl;
        raise(SIGTERM);        
    }

    FILE *fp = fopen((outputStem).c_str(), "wb");
    fprintf(fp, "P6\n");
    fprintf(fp, "%d %d\n", image.width, image.height);
    fprintf(fp, "255\n");
    fwrite(image.data.get(), 1, image.size(), fp);
    fclose(fp);
}

int
detectMorph(
        std::shared_ptr<Interface> &implPtr,
        const string &inputFile,
        const string &outputLog,
        Action action,
        const string &outputDir)
{
    /* Read input file */
    ifstream inputStream(inputFile);
    if (!inputStream.is_open()) {
        cerr << "Failed to open stream for " << inputFile << "." << endl;
        raise(SIGTERM);
    }

    /* Open output log for writing */
    ofstream logStream(outputLog);
    if (!logStream.is_open()) {
        cerr << "Failed to open stream for " << outputLog << "." << endl;
        raise(SIGTERM);
    }

    string line;
    ReturnStatus ret;

    if (action == Action::DetectNonScannedMorph ||
    	action == Action::DetectScannedMorph ||
    	action == Action::DetectUnknownMorph) {
        logStream << "image isMorph score returnCode" << endl;
    } else if (action == Action::DetectNonScannedMorphWithProbeImg ||
    	action == Action::DetectScannedMorphWithProbeImg ||
    	action == Action::DetectUnknownMorphWithProbeImg || 
        action == Action::DetectNonScannedMorphWithProbeImgAndMeta ||
        action == Action::DetectScannedMorphWithProbeImgAndMeta ||
        action == Action::DetectUnknownMorphWithProbeImgAndMeta) {
        logStream << "image probeImage isMorph score returnCode" << endl;
    } else if (action == Action::Demorph) {
        logStream << "image outputSubject1 outputSubject2 isMorph score returnCode" << endl;
    } else if (action == Action::DemorphDifferentially) {
        logStream << "image probeImage outputSubject isMorph score returnCode" << endl;
    }
    

    std::map<std::string, FRVT_MORPH::SubjectMetadata::Sex> mapStringToSexLabel =
    {
        { "UNKNOWN", FRVT_MORPH::SubjectMetadata::Sex::Unknown },
        { "FEMALE", FRVT_MORPH::SubjectMetadata::Sex::Female },
        { "MALE", FRVT_MORPH::SubjectMetadata::Sex::Male },
    };

    while (std::getline(inputStream, line)) {
        Image image, probeImage;
        auto imgs = split(line, ' ');
        if (!readImage(imgs[0], image)) {
            cerr << "Failed to load image file: " << imgs[0] << "." << endl;
            raise(SIGTERM);
        }
        bool isMorph = false;
        double score = -1.0;
        FRVT::Image outputSubject1, outputSubject2;

        if (action == Action::DetectNonScannedMorph ||
                action == Action::DetectScannedMorph ||
                action == Action::DetectUnknownMorph) {
            ret = implPtr->detectMorph(image, mapActionToMorphLabel[action], isMorph, score);
        } else if (action == Action::DetectNonScannedMorphWithProbeImg ||
                action == Action::DetectScannedMorphWithProbeImg ||
                action == Action::DetectUnknownMorphWithProbeImg) {
            if (!readImage(imgs[1], probeImage)) {
                cerr << "Failed to load image file(s): " << imgs[1] << "." << endl;
                raise(SIGTERM);
            }
            ret = implPtr->detectMorphDifferentially(image, mapActionToMorphLabel[action], probeImage, isMorph, score);
        } else if (action == Action::DetectNonScannedMorphWithProbeImgAndMeta ||
                action == Action::DetectScannedMorphWithProbeImgAndMeta ||
                action == Action::DetectUnknownMorphWithProbeImgAndMeta) {
            if (!readImage(imgs[1], probeImage)) {
                cerr << "Failed to load image file(s): " << imgs[1] << "." << endl;
                raise(SIGTERM);
            }
            FRVT_MORPH::SubjectMetadata meta(mapStringToSexLabel[imgs[2]], std::stoi(imgs[3]), std::stoi(imgs[4])); 
            ret = implPtr->detectMorphDifferentially(image, mapActionToMorphLabel[action], probeImage, meta, isMorph, score);
        } else if (action == Action::Demorph) {
            ret = implPtr->demorph(image, outputSubject1, outputSubject2, isMorph, score); 
        } else if (action == Action::DemorphDifferentially) {
            if (!readImage(imgs[1], probeImage)) {
                cerr << "Failed to load image file(s): " << imgs[1] << "." << endl;
                raise(SIGTERM);
            }
            ret = implPtr->demorphDifferentially(image, probeImage, outputSubject1, isMorph, score);
        }

        /* If function is not implemented, clean up and exit */
        if (ret.code == ReturnCode::NotImplemented) {
            break;
        }

        /* Write template stats to log */
        logStream << imgs[0] << " ";
        if (action == Action::DetectNonScannedMorphWithProbeImg ||
                action == Action::DetectScannedMorphWithProbeImg ||
                action == Action::DetectUnknownMorphWithProbeImg ||
                action == Action::DetectNonScannedMorphWithProbeImgAndMeta ||
                action == Action::DetectScannedMorphWithProbeImgAndMeta ||
                action == Action::DetectUnknownMorphWithProbeImgAndMeta)
            logStream << imgs[1] << " ";

        if (action == Action::Demorph) {
            auto stem = split(split(imgs[0], '/').back(), '.').front();
            std::string subj1 = stem + "_outputSubject1.ppm";
            std::string subj2 = stem + "_outputSubject2.ppm";
            writeImagePPM(outputSubject1, outputDir + "/" + subj1);
            writeImagePPM(outputSubject2, outputDir + "/" + subj2);

            logStream << subj1 << " " << subj2 << " ";
        } else if (action == Action::DemorphDifferentially) {
            auto stem = split(split(imgs[0], '/').back(), '.').front();
            std::string subj1 = stem + "_outputSubject.ppm";
            writeImagePPM(outputSubject1, outputDir + "/" + subj1);
            logStream << imgs[1] << " " << subj1 << " ";
        }
        
        logStream << isMorph << " "
                << score << " "
                << static_cast<std::underlying_type<ReturnCode>::type>(ret.code)
                << endl;
    }
    inputStream.close();

    /* Remove the input file */
    if( remove(inputFile.c_str()) != 0 )
        cerr << "Error deleting file: " << inputFile << endl;

    if (ret.code == ReturnCode::NotImplemented) {
        /* Remove the output file */
        logStream.close();
        if( remove(outputLog.c_str()) != 0 )
            cerr << "Error deleting file: " << outputLog << endl;
        return NOT_IMPLEMENTED;
    }
    return SUCCESS;
}

int
compare(
        std::shared_ptr<Interface> &implPtr,
        const string &inputFile,
        const string &scoresLog)
{
    /* Read probes */
    ifstream inputStream(inputFile);
    if (!inputStream.is_open()) {
        cerr << "Failed to open stream for " << inputFile << "." << endl;
        raise(SIGTERM);
    }

    /* Open scores log for writing */
    ofstream scoresStream(scoresLog);
    if (!scoresStream.is_open()) {
        cerr << "Failed to open stream for " << scoresLog << "." << endl;
        raise(SIGTERM);
    }
    /* header */
    scoresStream << "enrollImage verifImage score returnCode" << endl;

    /* Process each probe */
    string enroll, verif;
    Image enrollImage, verifImage;
    ReturnStatus ret;
    while (inputStream >> enroll >> verif) {
        if (!readImage(enroll, enrollImage)) {
            cerr << "Failed to load image file: " << enroll << "." << endl;
            raise(SIGTERM);
        }
        if (!readImage(verif, verifImage)) {
            cerr << "Failed to load image file: " << verif << "." << endl;
            raise(SIGTERM);
        }

        double similarity = -1.0;
        /* Call compare */
        ret = implPtr->compareImages(enrollImage, verifImage, similarity);

        /* If function is not implemented, clean up and exit */
        if (ret.code == ReturnCode::NotImplemented) {
            break;
        }

        /* Write to scores log file */
        scoresStream << enroll << " "
                << verif << " "
                << similarity << " "
                << static_cast<std::underlying_type<ReturnCode>::type>(ret.code)
                << endl;
    }
    inputStream.close();

    /* Remove the input file */
    if( remove(inputFile.c_str()) != 0 )
        cerr << "Error deleting file: " << inputFile << endl;

    if (ret.code == ReturnCode::NotImplemented) {
        /* Remove the output file */
        scoresStream.close();
        if( remove(scoresLog.c_str()) != 0 )
            cerr << "Error deleting file: " << scoresLog << endl;
        return NOT_IMPLEMENTED;
    }

    return SUCCESS;
}

void usage(const string &executable)
{
    cerr << "Usage: " << executable <<
            " detectNonScannedMorph"
            "|detectScannedMorph"
            "|detectUnknownMorph"
            "|detectNonScannedMorphWithProbeImg"
            "|detectScannedMorphWithProbeImg"
            "|detectUnknownMorphWithProbeImg"
            "|detectNonScannedMorphWithProbeImgAndMeta"
            "|detectScannedMorphWithProbeImgAndMeta"
            "|detectUnknownMorphWithProbeImgAndMeta"
            "|compare"
            "|demorph"
            "|demorphDifferentially -c configDir "
            "-o outputDir -h outputStem -i inputFile -t numForks" << endl;
    exit(EXIT_FAILURE);
}

int
main(
        int argc,
        char* argv[])
{
    auto exitStatus = SUCCESS;

    uint16_t currAPIMajorVersion{5},
		currAPIMinorVersion{0},
		currStructsMajorVersion{3},
		currStructsMinorVersion{0};

    /* Check versioning of both frvt_structs.h and API header file */
	if ((FRVT::FRVT_STRUCTS_MAJOR_VERSION != currStructsMajorVersion) ||
			(FRVT::FRVT_STRUCTS_MINOR_VERSION != currStructsMinorVersion)) {
		cerr << "[ERROR] You've compiled your library with an old version of the frvt_structs.h file: version " <<
		    FRVT::FRVT_STRUCTS_MAJOR_VERSION << "." <<
		    FRVT::FRVT_STRUCTS_MINOR_VERSION <<
		    ".  Please re-build with the latest version: " <<
		    currStructsMajorVersion << "." <<
	   	    currStructsMinorVersion << "." << endl;
		return (FAILURE);
	}

	if ((FRVT_MORPH::API_MAJOR_VERSION != currAPIMajorVersion) ||
			(FRVT_MORPH::API_MINOR_VERSION != currAPIMinorVersion)) {
		std::cerr << "[ERROR] You've compiled your library with an old version of the API header file: " <<
		    FRVT_MORPH::API_MAJOR_VERSION << "." <<
		    FRVT_MORPH::API_MINOR_VERSION <<
		    ".  Please re-build with the latest version: " <<
		    currAPIMajorVersion << "." <<
		    currStructsMinorVersion << "." << endl;
		return (FAILURE);
	}

    int requiredArgs = 2; /* exec name and action */
    if (argc < requiredArgs)
        usage(argv[0]);

    string actionstr{argv[1]},
        configDir{"config"},
        configValue{""},
        outputDir{"output"},
        outputFileStem{"stem"},
        inputFile;
    int numForks = 1;

    for (int i = 0; i < argc - requiredArgs; i++) {
        if (strcmp(argv[requiredArgs+i],"-c") == 0)
            configDir = argv[requiredArgs+(++i)];
        else if (strcmp(argv[requiredArgs+i],"-v") == 0)
            configValue = argv[requiredArgs+(++i)];
        else if (strcmp(argv[requiredArgs+i],"-o") == 0)
            outputDir = argv[requiredArgs+(++i)];
        else if (strcmp(argv[requiredArgs+i],"-h") == 0)
            outputFileStem = argv[requiredArgs+(++i)];
        else if (strcmp(argv[requiredArgs+i],"-i") == 0)
            inputFile = argv[requiredArgs+(++i)];
        else if (strcmp(argv[requiredArgs+i],"-t") == 0)
            numForks = atoi(argv[requiredArgs+(++i)]);
        else {
            cerr << "Unrecognized flag: " << argv[requiredArgs+i] << endl;;
            usage(argv[0]);
        }
    }

    Action action = mapStringToAction[actionstr];
    switch (action) {
        case Action::DetectNonScannedMorph:
        case Action::DetectScannedMorph:
        case Action::DetectUnknownMorph:
        case Action::DetectNonScannedMorphWithProbeImg:
        case Action::DetectScannedMorphWithProbeImg:
        case Action::DetectUnknownMorphWithProbeImg:
        case Action::DetectNonScannedMorphWithProbeImgAndMeta:
        case Action::DetectScannedMorphWithProbeImgAndMeta:
        case Action::DetectUnknownMorphWithProbeImgAndMeta:
        case Action::Compare:
        case Action::Demorph:
        case Action::DemorphDifferentially:
            break;
        default:
            cerr << "Unknown command: " << actionstr << endl;
            usage(argv[0]);
    }

    /* Get implementation pointer */
    auto implPtr = Interface::getImplementation();
    /* Initialization */
    auto ret = implPtr->initialize(configDir, configValue);
    if (ret.code != ReturnCode::Success) {
        cerr << "initialize() returned error code: "
                << ret.code << "." << endl;
        return FAILURE;
    }

    /* Split input file into appropriate number of splits */
    vector<string> inputFileVector;
    if (splitInputFile(inputFile, outputDir, numForks, inputFileVector) != SUCCESS) {
        cerr << "An error occurred with processing the input file." << endl;
        return FAILURE;
    }

    bool parent = false;
	int i = 0;
    for (auto &inputFile : inputFileVector) {
		/* Fork */
		switch(fork()) {
		case 0: /* Child */
            switch (action) {
                case Action::DetectNonScannedMorph:
                case Action::DetectScannedMorph:
                case Action::DetectUnknownMorph:
                case Action::DetectNonScannedMorphWithProbeImg:
                case Action::DetectScannedMorphWithProbeImg:
                case Action::DetectUnknownMorphWithProbeImg:
                case Action::DetectNonScannedMorphWithProbeImgAndMeta:
                case Action::DetectScannedMorphWithProbeImgAndMeta:
                case Action::DetectUnknownMorphWithProbeImgAndMeta:
                case Action::Demorph:
                case Action::DemorphDifferentially:
                    return detectMorph(
                            implPtr,
                            inputFile,
                            outputDir + "/" + outputFileStem + ".log." + to_string(i),
                            action,
                            outputDir);
                case Action::Compare:
                    return compare(
                            implPtr,
                            inputFile,
                            outputDir + "/" + outputFileStem + ".log." + to_string(i));
				default:
					return FAILURE;
            }
		case -1: /* Error */
			cerr << "Problem forking" << endl;
			break;
		default: /* Parent */
			parent = true;
			break;
		}
		i++;
	}

    /* Parent -- wait for children */
    if (parent) {
        while (numForks > 0) {
            int stat_val;
            pid_t cpid;

            cpid = wait(&stat_val);
            if (WIFEXITED(stat_val)) { exitStatus = WEXITSTATUS(stat_val); }
            else if (WIFSIGNALED(stat_val)) {
                cerr << "PID " << cpid << " exited due to signal " <<
                        WTERMSIG(stat_val) << endl;
                exitStatus = FAILURE;
            } else {
                cerr << "PID " << cpid << " exited with unknown status." << endl;
                exitStatus = FAILURE;
            }
            numForks--;
        }
    }

    return exitStatus;
}
