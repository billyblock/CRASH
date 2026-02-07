#include <iostream>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <dirent.h>
#include <unordered_map>
#include "readline/readline.h"
#include "readline/history.h"
#include <filesystem>
#include <fstream>
#include <signal.h>
#include <wordexp.h>

using namespace std;

// I declare these up here to avoid order of function issues.
void execute(const char *line);
string getPrompt();
auto pid = -1;

// https://en.wikipedia.org/wiki/Levenshtein_distance
// https://medium.com/@ethannam/understanding-the-levenshtein-distance-equation-for-beginners-c4285a5604f0
int levenshteinDistance(string target, string comparer, int tSize, int cSize, unordered_map<string, int> &map)
{
    // Create a key for the map
    string key = to_string(tSize) + "-" + to_string(cSize);
    // If we have already done this before, return the same value.
    if (map.count(key))
    {
        return map[key];
    }
    // The next 2 if's are computing the difference in amount of letters
    // if target is empty, return comparer size
    if (tSize == 0)
    {
        return cSize;
    }
    // if comparer is empty, return target size
    if (cSize == 0)
    {
        return tSize;
    }

    // If the characters are the same, nothing happens, go to the next char
    if (target[tSize - 1] == comparer[cSize - 1])
    {
        map[key] = levenshteinDistance(target, comparer, tSize - 1, cSize - 1, map);
        return map[key];
    }
    // Otherwise, compute amount of inserts, removes, and replacements.
    int inserts = levenshteinDistance(target, comparer, tSize, cSize - 1, map);
    int removes = levenshteinDistance(target, comparer, tSize - 1, cSize, map);
    int replaces = levenshteinDistance(target, comparer, tSize - 1, cSize - 1, map);
    // Whatever one gets us closer, return that.
    map[key] = 1 + min(inserts, min(removes, replaces));
    return map[key];
}
// Returns the string closer to target
string getCloserString(string target, string s1, string s2)
{
    // hashmaps for local caching, learned from andy in algorithms
    unordered_map<string, int> map1;
    unordered_map<string, int> map2;
    // get the distance between each string and the target
    int dist1 = levenshteinDistance(target, s1, target.size(), s1.size(), map1);
    int dist2 = levenshteinDistance(target, s2, target.size(), s2.size(), map2);
    if (dist1 < dist2)
        return s1;
    return s2;
}

// Finds the given file, returns empty string if not found
string findFile(char *target, string current, string &closest)
{
    // Open the directory
    auto dir = opendir(current.c_str());
    if (dir == nullptr)
        return "";

    struct dirent *ent;
    // For each directory, read each file
    while ((ent = readdir(dir)) != nullptr)
    {
        // if its a directory, go in that directory to find it
        if (ent->d_type == DT_DIR)
        {
            std::string name = ent->d_name;
            if (name != "." && name != "..")
            {
                string found = findFile(target, current + "/" + name, closest);
                if (!found.empty())
                {
                    closedir(dir);
                    return found;
                }
            }
        }

        // if its a file, check if it's the right one
        if (ent->d_type == DT_REG)
        {
            string fileName = ent->d_name;
            string tString = target;
            closest = getCloserString(tString, closest, fileName);

            if (fileName == target)
            {
                string fullPath;
                fullPath = current + "/" + fileName;
                closedir(dir);
                closest = "";
                return fullPath;
            }
        }
    }

    closedir(dir);
    return "";
}

// Parses aString on delim, stores it in pWords and sWords.
// pWords is a char* equivalent of sWords.
void parseOnDelim(const char *aString, vector<char *> &pWords, vector<string> &sWords, char delim)
{
    string word;
    // for every letter in line, if its a delim, remove all delims until next letter
    for (int i = 0; i < strlen(aString); i++)
    {
        if (aString[i] == delim)
        {
            sWords.push_back(word);
            word = "";
            // skip all delims
            while (aString[i + 1] == delim)
            {
                i++;
            }
        }
        else
        {
            word += aString[i];
        }
    }
    // push the final word on
    sWords.push_back(word);

    // Turn the vector of strings into a vector of char* for exec
    for (int i = 0; i < sWords.size(); i++)
    {
        // string to char* conversion:
        // https://stackoverflow.com/questions/7352099/stdstring-to-char
        pWords.push_back(sWords[i].data());
    }
    pWords.push_back(nullptr);
}

void parent(int &pid, bool background)
{
    int status;
    // if theres a background process, keep going.
    if (background)
    {
        waitpid(pid, &status, WNOHANG);
    }
    else
    {
        waitpid(pid, &status, 0);
    }
}

// Runs each line of the file as if it was a command.
void runFile(string fileName)
{
    string line;
    ifstream file(fileName);
    while (getline(file, line))
    {
        execute(line.c_str());
    }
}

void signalHandler(int sig)
{
    // if theres a kid
    if (pid != -1)
    {
        // kill the kid
        kill(pid, SIGTERM);
    }
    cout << endl;
    cout << getPrompt();
}

void initializeCRASH()
{
    read_history(".crashHistory");

    // only allow 100 lines of history
    stifle_history(100);

    runFile(".crash");

    // https://stackoverflow.com/questions/1641182/how-can-i-catch-a-ctrl-c-event
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    cout << "Hello, welcome to \e[38;5;196mCRASH! \e[0m(Crashes Randomly And Sometimes Hangs)" << endl;
}

void child(vector<char *> arguments, string line)
{
    // Execv needs char**, you cannot make a char* in a method or else it gets unallocated
    vector<char *> paths;
    // Strings are much easier to work with.
    vector<string> tempPaths;
    parseOnDelim(getenv("PATH"), paths, tempPaths, ':');

    // The string closest to what we are looking for
    string closest = "";
    string pathToExe;
    for (char *path : paths)
    {
        // for some reason if paths contains a nullptr, the loop doesnt stop automatically
        if (path == nullptr)
            break;

        string p2 = path;
        // My windows machine has a path to basically the entire file system,
        // if it's one of those, skip it
        // this compare takes a start, length, and string that you're comparing
        if (p2.compare(0, 5, "/mnt/") == 0)
            continue;

        // FindFile return "" if not found
        pathToExe = findFile(arguments[0], path, closest);

        if (pathToExe != "")
            break;
    }
    execv(pathToExe.c_str(), arguments.data());
    // only reaches here if exec failed
    cout << "Command not found" << endl;
    cout << "Did you mean: " << closest << "?" << endl;
    exit(1);
}

// Sees if line contains aChar
bool contains(string line, char aChar)
{
    for (int i = 0; i < line.size(); i++)
    {
        if (line[i] == aChar)
        {
            return true;
        }
    }
    return false;
}
// removes every occurrence of toRemove from the back of the string
void removeFromBack(string &line, char toRemove)
{
    for (int i = line.size() - 1; i >= 0; i--)
    {
        if (line[i] == toRemove || line[i] == ' ')
        {
            line.pop_back();
        }
        else
        {
            return;
        }
    }
}

// removes all space from the front and back.
void trim(string &aString)
{
    // remove from front
    for (int i = 0; i < aString.size(); i++)
    {
        if (aString[i] == ' ')
        {
            // earase from index 0, 1 character
            aString.erase(0, 1);
        }
        else
        {
            break;
        }
    }
    // remove from back.
    for (int i = aString.size() - 1; i >= 0; i--)
    {
        if (aString[i] == ' ')
        {
            aString.pop_back();
        }
        else
        {
            break;
        }
    }
}

void trim(char *aString)
{
    string sString(aString);
    trim(sString);
    // copy back to char*
    strcpy(aString, sString.c_str());
}

string getPrompt()
{
    const char *user = getenv("USER");
    // https://www.geeksforgeeks.org/cpp/how-to-get-current-directory-in-cpp/
    char buf[4096];
    getcwd(buf, 4096);
    // got from bash prompt generator, added in actual escape characters
    return "\e[38;5;196mCRASH-\e[92m" + string(user) + " \e[0m" + string(buf) + "$ ";
}

// http://www.gnu.org/software/libc/manual/html_node/Wordexp-Example.html
string wordExpand(const char *line)
{
    wordexp_t result;
    wordexp(line, &result, 0);
    string newLine;
    for (int i = 0; i < result.we_wordc; i++)
    {
        newLine += string(result.we_wordv[i]);
        // add a space at the end
        if (i != result.we_wordc - 1)
            newLine += " ";
    }
    wordfree(&result);
    return newLine;
}

void execute(const char *line)
{
    // Some functions need char*, others need strings.
    vector<char *> arguments;
    vector<string> tempArgs; // to keep the memory in use, allocate it here

    // Set env var
    if (contains(line, '='))
    {
        parseOnDelim(line, arguments, tempArgs, '=');
        setenv(arguments[0], arguments[1], 1);
        return;
    }

    // Background process
    bool background = false;
    string sLine(line);
    if (contains(line, '&'))
    {
        removeFromBack(sLine, '&');
        line = sLine.c_str();
        background = true;
    }

    // queue commands
    if (contains(line, ';'))
    {
        // Parse the command on ; and run each smaller command.
        vector<char *> semiArgs;
        vector<string> semiTempArgs;
        parseOnDelim(line, semiArgs, semiTempArgs, ';');
        for (string cmd : semiTempArgs)
        {
            trim(cmd);
            execute(cmd.c_str());
        }
        return;
    }

    // Always expand the line
    string expandedLine = wordExpand(line);
    line = expandedLine.c_str();

    parseOnDelim(line, arguments, tempArgs, ' ');
    string cmd(arguments[0]);

    if (cmd == "exit")
    {
        exit(0);
    }
    if (cmd == "cd")
    {
        if (chdir(arguments[1]) == -1)
            perror("Cannot cd");
        return;
    }
    if (cmd == ".")
    {
        runFile(tempArgs[1]);
        return;
    }
    pid = fork();
    if (pid < 0)
    {
        cout << "fork failed" << endl;
        exit(1);
    }
    else if (pid == 0)
    {
        child(arguments, line);
        exit(1);
    }
    else
    {
        parent(pid, background);
        // child(arguments);
    }
}

int main(int argc, char **argv)
{
    initializeCRASH();

    const char *line;
    
    // https://www.geeksforgeeks.org/cpp/how-to-get-current-directory-in-cpp/
    char buf[4096];
    getcwd(buf, 4096);
    string file = ".crashHistory";
    string fileDir = buf + file;

    while ((line = readline(getPrompt().c_str())) != nullptr)
    {
        add_history(line);
        write_history(fileDir.c_str());
        execute(line);
        free((char *)line); // free needs a non const pointer
    }
}