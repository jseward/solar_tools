using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace csv_to_text
{
    class Program
    {
        static int Main(string[] args)
        {
            var options = new Options();
            if (CommandLine.Parser.Default.ParseArgumentsStrict(args, options))
            {
                try
                {
                    var textMap = new TextMap();
                    textMap.LoadFromCsv(options.Input);
                    textMap.SaveToText(options.Ouptut);
                }
                catch (Exception ex)
                {
                    Console.WriteLine(ex.ToString());
                    return 1;
                }
            }

            return 0;
        }
    }
}
