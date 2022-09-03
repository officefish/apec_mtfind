/******************************************************************************

Test quiz for APEC

developer: Sergey Inozemcev

compile: g++ mtfind.cpp -ltbb -pthread -std=c++17 -o mtfind

run: ./mtfind -a=1 input.txt "?ad"

*-a or --algorithm - is searcher algorithm used for search
0 - (default) std::default_searcher
1 - std::boyer_moore_searcher
2 - std::boyer_moore_horspool_searcher

* only two "?"" for mask can be used, 
for upgrade mask numbers we should refactor source code of std searchers code

*******************************************************************************/

#include <algorithm>
#include <functional>
#include <iterator>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <set>

#include <fstream>
#include <filesystem>

#include <execution>
#include <future>
#include <thread>
#include <chrono>
#include <cassert>

class CmdArguments {
private:
    std::string m_sPath;
    std::string m_sMask;
    uint8_t m_nAlgorithm;

public:

    CmdArguments() : m_nAlgorithm(0) {}    
    void parse (int argc, char* argv[]) 
    {
        
        assert((argc >= 3 && argc < 5) && "Invalid number of arguments");

        std::vector<std::string> args(argv + 1, argv + argc);

        for (const auto& arg : args) 
        {
            if (arg == "-a=1" || arg == "--algorithm=1") 
            {
                m_nAlgorithm = 1;
            }
            else if (arg == "-a=2" || arg == "--algorithm=2") 
            {
                m_nAlgorithm = 2;
            }
        }

        m_sPath = args[args.size() - 2];
        m_sMask = args[args.size() - 1];
    }

    std::string getPath() {return m_sPath; }
    std::string getMask() {return m_sMask; }
    uint8_t getAlgorithm() {return m_nAlgorithm; }
};

class NeedleMaker {
private:
    std::vector<char> ascii;
    std::vector<char>& getASCII()
    {
        if (ascii.size()) 
            return ascii;

        for (size_t i = 32; i < 127; ++i)
        {
            // if char is not "?"
            if (i != 63) 
                ascii.push_back(i);
        }
        return std::ref(ascii);
    }
    
public:
    bool isValidMask (std::string mask)
    {
        if (mask.length() > 100) return false;

        /* mask limit is 2 "?" */
        uint8_t anyCounter = 0;
        for (auto& character : mask)
        {
            // if char is not "?"
            if (character == 63) 
                anyCounter++;
        }
        return anyCounter > 2 ? false : true;
    }

    std::vector<std::string> makeFromMask (std::string mask)
    {
        std::vector<std::string> needles;
        for(std::string::iterator it = mask.begin(); it != mask.end(); ++it) 
        {
            // if char is "?"
            if (*it == 63) 
            {
                int index = it - mask.begin();
                for (auto character : getASCII())
                {
                    std::string needle = mask; 
                    needle[index] = character;
                    std::vector<std::string> characterNeedles = makeFromMask(needle);
                    needles.insert( needles.end(), characterNeedles.begin(), characterNeedles.end() );
                }
                
            }
        }

        if (!needles.size())
            needles.push_back(mask);

        return needles;
    }
    
};

class Separator {
private:
    std::ifstream n_File;

public:
    
    void open (std::string path)
    {
        
        auto get_full_path = [](std::string path) -> std::string
        {
            std::string full_path;
            full_path += std::filesystem::current_path();
            full_path += "/";
            full_path += path;
            return full_path;
        };
        
        auto full_path = get_full_path(path);

        n_File.open(full_path, std::ios::in);
        
    }

    std::vector<std::string> getLines ()
    {
        std::vector<std::string> numLines; 
        std::string line;

        while(getline(n_File, line))
            numLines.push_back(line);
        
        return numLines;
    }

    bool isFileOpen()
    {
        return n_File.is_open();
    }

    void closeFile() 
    {
        n_File.close();
    }    

};

class Token {

private:
    std::string m_sNeedle;
    uint16_t m_nSymbolIndex;
    uint32_t m_nLineIndex;

public:
    Token(std::string needle, uint16_t symbolIndex, uint32_t lineIndex)
    : m_sNeedle(needle), m_nSymbolIndex(symbolIndex), m_nLineIndex(lineIndex)
    {}

    bool operator< (const Token &other) const {
        return m_nLineIndex < other.m_nLineIndex;
    }

    friend std::ostream& operator<<(std::ostream& os, const Token& token);

};

std::ostream& operator<<(std::ostream& os, const Token& token)
{
    os << token.m_nLineIndex << ' ' << token.m_nSymbolIndex << ' ' << token.m_sNeedle;
    return os;
}



class Searcher {

    typedef std::function
    <std::pair
        <std::string::iterator,std::string::iterator>
        (std::string::iterator, std::string::iterator)
    > SearchPattern;

private:

    Searcher(Searcher&& searcher);
    Searcher(const Searcher& searcher);

    std::string m_sNeedle;
    std::string m_sSource;

    uint32_t m_nLineIndex;
    
    SearchPattern fn_Searcher;

    std::string::iterator fn_searchNextCursor (std::string::iterator& begin, std::string::iterator& end)
    {
        return std::search(begin, end, fn_Searcher);        
    }

public:
    Searcher (std::string source, std::string needle, uint32_t lineIndex, uint8_t algorithm) 
    {
        switch (algorithm)
        {
            case 1: fn_Searcher = std::boyer_moore_searcher(
                needle.begin(), needle.end());
            case 2: fn_Searcher = std::boyer_moore_horspool_searcher(
                needle.begin(), needle.end()); 
            default:
                fn_Searcher = std::default_searcher(
                needle.begin(), needle.end()); 
        }

        m_sNeedle = needle;
        m_sSource = source;

        m_nLineIndex = lineIndex;
    }

    std::vector<Token> search()
    {
        std::vector<Token> rg_Tokens;

        auto cursor = m_sSource.begin();
        auto beginLine = m_sSource.begin();
        auto endLine = m_sSource.end();

        while(cursor != endLine)
        {
            cursor = fn_searchNextCursor(cursor, endLine);
            
            if (cursor != endLine) 
            {
                auto symbolIndex = cursor - beginLine;
                rg_Tokens.emplace_back(m_sNeedle, symbolIndex + 1, m_nLineIndex);
                cursor++;
            }
        }
        return rg_Tokens;
    }
};


class Invoker 
{

private:
    Invoker(Invoker&& invoker);
    Invoker(const Invoker& invoker);

    std::string m_sPath;
    std::string m_sMask;

    std::vector<std::string> m_rgLines;
    std::vector<std::string> m_rgNeedles;
    std::set<Token> m_rgTokens;

    uint8_t m_nAlgorithm;

public:
    Invoker(const std::string& path, const std::string& mask, uint8_t algorithm) 
    : m_sPath(path), m_sMask(mask), m_nAlgorithm(algorithm) {}

    void search_preproduction()
    {
        /* Produce needles in separate thread */
    
        bool getNeedlesStatus = false;

        std::thread getNeedles_thread(
            [](std::string mask, std::vector<std::string>& needles, bool& status)
            {
                NeedleMaker maker;
                if (!maker.isValidMask(mask)) return;
                needles = maker.makeFromMask(mask);
                status = true;
            }, m_sMask, std::ref(m_rgNeedles), std::ref(getNeedlesStatus));

        
        /* Produce separate lines from file in separate thread */
        
        bool getLinesStatus = false;

        std::thread getLines_thread(
            [](std::string path, std::vector<std::string>& lines, bool& status)
            {
                Separator separator;
                separator.open(path);
                if (!separator.isFileOpen()) return;
                lines = separator.getLines();
                separator.closeFile();
                status = true;
            }, m_sPath, std::ref(m_rgLines), std::ref(getLinesStatus)); 

        getNeedles_thread.join();
        getLines_thread.join();

        /* If both thread process status is correct - continue */
        assert (getNeedlesStatus && "Mask is incorrect");
        assert (getLinesStatus && "File path is incorrect");   
    }

    void parallel_search()
    {

        auto collectTokens = [](std::set<Token>& tokens, const std::vector<std::vector<Token>>& lineTokensList) -> void
        {
            for (auto & lineTokens : lineTokensList)
            {
                for (auto & token : lineTokens)
                    tokens.insert(token); 
            }
        };

        while (!m_rgLines.empty())
        {       
            std::string line = m_rgLines.back();
            uint16_t lineIndex = m_rgLines.size();

            m_rgLines.pop_back();
            
            std::vector<std::vector<Token>> lineTokens(m_rgNeedles.size());

            auto searchTokens = [&](std::string needle) -> std::vector<Token> 
            {
                Searcher searcher(line, needle, lineIndex, m_nAlgorithm);
                return searcher.search();
            };

            std::transform(std::execution::par, // parallel thread search  
            m_rgNeedles.begin(), m_rgNeedles.end(), lineTokens.begin(),
            searchTokens);

            collectTokens(m_rgTokens, lineTokens);
        }  
    }

    void output()
    {
        std::cout << m_rgTokens.size() << "\n";
        for (auto& token : m_rgTokens)
            std::cout << token << "\n";
    }

};


int main(int argc, char *argv[])
{
    /* Parse command line arguments */
    CmdArguments cmdArgs;
    cmdArgs.parse(argc, argv);

    std::string path = cmdArgs.getPath();
    std::string mask = cmdArgs.getMask();
    uint8_t algorithm = cmdArgs.getAlgorithm();

    /* Parallel search using invoker */
    Invoker invoker(path, mask, algorithm);
    invoker.search_preproduction();
    invoker.parallel_search();
    invoker.output(); 

    return 0;
}