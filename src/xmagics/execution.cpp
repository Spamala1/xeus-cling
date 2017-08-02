/***************************************************************************
* Copyright (c) 2016, Johan Mabille, Loic Gouarin and Sylvain Corlay       *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/   
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "cling/Interpreter/Value.h"
#include "cling/Interpreter/Exception.h"
#include "cling/MetaProcessor/MetaProcessor.h"
#include "cling/Utils/Output.h"

#include "execution.hpp"

namespace xeus
{
    timeit::timeit(cling::MetaProcessor* p):m_processor(p){
        cling::Value result;
        cling::Interpreter::CompilationResult compilation_result;

        m_processor->process("#include <chrono>", compilation_result, &result);

        std::string init_timeit = "auto _t0 = std::chrono::high_resolution_clock::now();\n";
        init_timeit += "auto _t1 = std::chrono::high_resolution_clock::now();\n";           
        m_processor->process(init_timeit.c_str(), compilation_result, &result);

    }

    std::string timeit::inner(std::size_t number, std::string const & code) const
    {
        std::string timeit_code = "";
        timeit_code += "_t0 = std::chrono::high_resolution_clock::now();\n";
        timeit_code += "for(std::size_t _i=0; _i<" + std::to_string(number) + ";++_i){\n";
        timeit_code += "   " + code + "\n";
        timeit_code += "}\n";
        timeit_code += "_t1 = std::chrono::high_resolution_clock::now();\n";
        timeit_code += "std::chrono::duration<double>(_t1-_t0).count();";
        return timeit_code;
    }

    std::string timeit::_format_time(double timespan, std::size_t precision) const
    {
        std::vector<std::string> units{"s", "ms", "us", "ns"};
        std::vector<double> scaling{1, 1e3, 1e6, 1e9};
        std::ostringstream output;
        int order;

        if (timespan > 0.0)
            order = std::min(-static_cast<int>(std::floor(std::floor(std::log10(timespan))/3)), 3);
        else
            order = 3;
        output.precision(precision);
        output << timespan * scaling[order] << " " << units[order];
        return output.str();
    }

    void timeit::execute(std::string & options, std::string & code) const
    {
        auto opts = parse_opts(options, "n:r:p:qo");

        std::size_t number = (opts.find("n") != opts.end())? std::atoi(opts["n"].c_str()): 0ul;
        std::size_t default_repeat = 7;
        std::size_t repeat = (opts.find("r") != opts.end())? std::atoi(opts["r"].c_str()): default_repeat;
        std::size_t precision = (opts.find("p") != opts.end())? std::atoi(opts["p"].c_str()): 3;
        bool quiet = (opts.find("q") != opts.end());
        bool return_result = (opts.find("o") != opts.end());

        if (trim(code).empty())
            return;

        cling::Value result;
        cling::Interpreter::CompilationResult compilation_result;

        auto errorlevel = 0;
        try
        {
            if (number == 0)
                for(std::size_t n=0; n<10; ++n)
                {
                    number = std::pow(10, n);
                    std::string timeit_code = inner(number, code);
                    errorlevel = m_processor->process(timeit_code.c_str(), compilation_result, &result);
                    if (result.simplisticCastAs<double>() >= 0.2)
                        break;
                }

            std::vector<double> all_runs;
            double mean = 0;
            double stdev = 0;
            for(std::size_t r=0; r<repeat; ++r)
            {
                std::string timeit_code = inner(number, code);
                errorlevel = m_processor->process(timeit_code.c_str(), compilation_result, &result);
                all_runs.push_back(result.simplisticCastAs<double>()/number);
                mean += all_runs.back();
            }
            mean /= repeat;
            for(std::size_t r=0; r<repeat; ++r)
                stdev += (all_runs[r] - mean)*(all_runs[r] - mean);
            stdev = std::sqrt(stdev/repeat);
            auto minmax = std::minmax_element(all_runs.begin(), all_runs.end());
            std::cout << _format_time(mean, precision) << " +- " << _format_time(stdev, precision);
            std::cout << " per loop (mean +- std. dev. of " << repeat << " run" << ((repeat==1)? ", ":"s ");
            std::cout << number << " loop" << ((number==1)? "":"s") << " each)\n";             
        }
        catch (cling::InterpreterException& e)
        {
            if (!e.diagnose())
            {
                std::cerr << "Caught an interpreter exception!\n"
                       << e.what() << '\n';
            }
        }
        catch (std::exception& e)
        {
            std::cerr << "Caught a std::exception!\n"
                   << e.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "Exception occurred. Recovering...\n";
        }
    }
}