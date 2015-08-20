using CsvHelper;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace csv_to_text
{
    class TextMap
    {
        private Dictionary<string, string> _map;

        public TextMap()
        {
            _map = new Dictionary<string, string>();
        }

        public void LoadFromCsv(string path)
        {
            _map.Clear();

            using (var tr = File.OpenText(path))
            using (var cr = new CsvReader(tr))
            {
                while (cr.Read())
                {
                    string id = cr.GetField<string>(0);
                    string value = cr.GetField<string>(1);
                    _map.Add(id, value);
                }
            }
        }

        public void SaveToText(string path)
        {
            using (FileStream fs = File.Open(path, FileMode.Create))
            using (StreamWriter sw = new StreamWriter(fs))
            using (JsonWriter jw = new JsonTextWriter(sw))
            {
                jw.Formatting = Formatting.Indented;

                JsonSerializer serializer = new JsonSerializer();
                serializer.Serialize(jw, new { text = _map.Select(kvp => new { id = kvp.Key, value = kvp.Value }).ToArray() });
            }
        }
    }
}
