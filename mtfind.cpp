/******************************************************************************

Test quiz for APEC

developer: Sergey Inozemcev

compile: g++ mtfind.cpp -pthread -std=c++17 -o mtfind

run: ./mtfind -a=1 loren_ipsum.txt "someMask"

*-a or --algorithm - is searcher algorithm used for search
0 - (default) std::default_searcher
1 - std::boyer_moore_searcher
2 - std::boyer_moore_horspool_searcher

*******************************************************************************/

#include <algorithm>
#include <functional>
#include <iterator>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
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

        for (size_t i = 32; i < 128; ++i)
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
};


class Searcher {

    typedef std::function
    <std::pair
        <std::string::iterator,std::string::iterator>
        (std::string::iterator, std::string::iterator)
    > SearchPattern;

private:

    std::string::iterator m_itCursor;
    std::string::iterator m_itBeginLine;
    std::string::iterator m_itEndLine;
    std::string m_sNeedle;

    uint32_t m_nLineIndex;
    
    SearchPattern fn_Searcher;

    std::string::iterator fn_searchNextCursor (std::string::iterator begin, std::string::iterator end)
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
        m_itCursor = source.begin();
        m_itBeginLine = source.begin();
        m_itEndLine = source.end();

        m_nLineIndex = lineIndex;
    }

    Searcher(Searcher&& searcher)
    {
        m_sNeedle = searcher.m_sNeedle;
        m_itCursor = searcher.m_itCursor;
        m_itBeginLine = searcher.m_itBeginLine;
        m_itEndLine = searcher.m_itEndLine;
        m_nLineIndex = searcher.m_nLineIndex;
        fn_Searcher = searcher.fn_Searcher;
    }

    
    Searcher(const Searcher& searcher)
    {
        m_sNeedle = searcher.m_sNeedle;
        m_itCursor = searcher.m_itCursor;
        m_itBeginLine = searcher.m_itBeginLine;
        m_itEndLine = searcher.m_itEndLine;
        m_nLineIndex = searcher.m_nLineIndex;
        fn_Searcher = searcher.fn_Searcher;
    }


    std::vector<Token> search()
    {
        std::vector<Token> rg_Tokens;
        while(m_itCursor != m_itEndLine)
        {
            m_itCursor = fn_searchNextCursor(m_itCursor, m_itEndLine);
            
            if (m_itCursor != m_itEndLine) 
            {
                auto symbolIndex = m_itCursor - m_itBeginLine;
                rg_Tokens.emplace_back(m_sNeedle, symbolIndex, m_nLineIndex);
                m_itCursor++;
            }
        }
        return rg_Tokens;
    }
};


class Invoker 
{

private:
    std::string m_sPath;
    std::string m_sMask;

    std::vector<std::string> m_rgLines;
    std::vector<std::string> m_rgNeedles;
    std::vector<Token> m_rgTokens;

    uint8_t m_nAlgorithm;

public:
    Invoker(const std::string& path, const std::string& mask, uint8_t algorithm) 
    : m_sPath(path), m_sMask(mask), m_nAlgorithm(algorithm) {}

    Invoker(Invoker&& invoker)
    {
        m_sPath = invoker.m_sPath;
        m_sMask = invoker.m_sMask;
        m_nAlgorithm = invoker.m_nAlgorithm;
    }

    Invoker(const Invoker& invoker)
    {
        m_sPath = invoker.m_sPath;
        m_sMask = invoker.m_sMask;
        m_nAlgorithm = invoker.m_nAlgorithm;
    }
    

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
        auto collectTokens = [](std::vector<Token>& tokens, const std::vector<std::vector<Token>>& lineTokensList) -> void
        {
            for (auto & lineTokens : lineTokensList)
            {
                std::copy (lineTokens.begin(), lineTokens.end(), std::back_inserter(tokens));
            }
        };

        while (!m_rgLines.empty())
        {       
            std::string line = m_rgLines.back();
            m_rgLines.pop_back();

            uint16_t lineIndex = m_rgLines.size();
            std::vector<std::vector<Token>> lineTokens(m_rgNeedles.size());

            auto searchTokens = [&](std::string needle) -> std::vector<Token> 
            {
                Searcher searcher(line, needle, lineIndex, 0);
                std::vector<Token> lineNeedleTokens = searcher.search();
                return lineNeedleTokens;
            };

            std::transform(std::execution::par, // parallel thread search  
            m_rgNeedles.begin(), m_rgNeedles.end(), lineTokens.begin(),
            searchTokens);

            collectTokens(m_rgTokens, lineTokens);
        }  
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

    Invoker invoker(path, mask, algorithm);
    invoker.search_preproduction();
    invoker.parallel_search();
    

    return 0;
}