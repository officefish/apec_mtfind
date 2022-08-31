/******************************************************************************

Test quiz for APEC

developer: Sergey Inozemcev

compile: g++ mtfind.cpp -std=c++17 -o mtfind
run: ./mtfind 

*******************************************************************************/

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include <future>
#include <thread>
#include <chrono>

class Separator {
private:
    std::ifstream n_File;

    //bool fn_Open; 
public:
    
    void open (std::string path)
    {
        
        auto full_path = [](std::string path) -> std::string
        {
            std::string _full_path;
            _full_path += std::filesystem::current_path();
            _full_path += "/";
            _full_path += path;
            return _full_path;
        };
        
        auto _path = full_path(path);
        auto loader = [_path]() -> bool
        {
            std::ifstream file;
            file.open(_path, std::ios::in);
            return file.is_open();
        };

        std::future<bool> openFileThread = std::async(std::launch::async, loader);
        std::future_status status;

        std::cout << "Start loading file: " << path << std::flush;
        
        while(true)
        {
            std::cout << "Loading ..." << std::flush;
            status = openFileThread.wait_for(std::chrono::milliseconds(1000));

            if (status == std::future_status::ready)
            {
                std::cout << "File loaded" << std::flush;
                break;
            }

            //if (status == std::future_status::)

        }
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
    Searcher (std::string source, std::string needle, uint32_t lineIndex, uint8_t algorithm = 0) 
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


int main()
{
    std::string in = "Lorem ipsum dolor sit amet, consectetur adipiscing elit,"
                     " sed do eiusmod tempor incididunt ut labore et dolore magna aliqua";
    std::string needle = "pisci";

    /*
        Получить ссылку на файл и разобрать его на строки в одном потоке
    */

    /*
        Разобрать маску на ключи поиска во втором потоке
    */

    Separator separator;
    separator.open("loren_ipsum.txt");

    /*
    auto it = std::search(in.begin(), in.end(),
                   std::boyer_moore_searcher(
                       needle.begin(), needle.end()));
    if(it != in.end())
        std::cout << "The string " << needle << " found at offset "
                  << it - in.begin() << '\n';
    else
        std::cout << "The string " << needle << " not found\n";
    */

    //Searcher searcher { in, needle };
    //auto tokens = searcher.search();

    //std::cout << tokens.size() << std::endl;

    return 0;
}