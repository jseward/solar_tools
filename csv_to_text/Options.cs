using CommandLine;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace csv_to_text
{
    class Options
    {
        [Option('i', "input", HelpText = "Input .csv file path", Required = true)]
        public string Input { get; set; }

        [Option('o', "output", HelpText = "Output .text file path", Required = true)]
        public string Ouptut { get; set; }
    }
}
