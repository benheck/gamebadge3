using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;

namespace Tiled_To_Gamebadge3
{

    public partial class Form1 : Form
    {
        ushort[,] exportMap = new ushort[15, 120];

        short[,] currentMap = new short[15, 120];

        string condoName;

        public Form1()
        {
            InitializeComponent(); 
        }




        private void button1_Click(object sender, EventArgs e)
        {
            System.Media.SystemSounds.Beep.Play();

            String fileName = GetFileName("CSV (*.csv)|*.csv");
            if (fileName != null)
            {
                filenameBox.Text = fileName;
                pathBox.Text = Path.GetDirectoryName(fileName);

                string path = filenameBox.Text;
                string[] pathArr = path.Split('\\');
                string[] fileArr = pathArr.Last().Split('.');
                string[] noExt = fileArr.First().Split('_');
                condoName = noExt.First().ToString();

                textBox_0.Text = condoName + "_pal0";
                textBox_1.Text = condoName + "_pal1";
                textBox_2.Text = condoName + "_pal2";
                textBox_3.Text = condoName + "_pal3";
                textBox_platform.Text = condoName + "_platform";

                statusLabel.Text = "LOADED";
            }

        }

        private String GetFileName(String filter)
        {
            OpenFileDialog dlg = new OpenFileDialog();
            dlg.Filter = filter;
            dlg.RestoreDirectory = true;
            if (filenameBox.Text.Length > 0)
            {
                dlg.InitialDirectory = GetCurrentFilePath();
            }
            if (dlg.ShowDialog(this) == DialogResult.OK)
            {
                return dlg.FileName;
            }
            else
            {
                return null;
            }
        }

        private String GetCurrentFilePath()
        {
            return filenameBox.Text.Substring(0, filenameBox.Text.LastIndexOf("\\") + 1);
        }

        private void buttonMap_Click(object sender, EventArgs e)
        {

            //Bud Game Palette 3 (B&W, condo background)
            currentMap = ConvertCSV(pathBox.Text + "\\" + textBox_3.Text + ".csv");        //Convert TILED CSV to a 2D short array

            for (int i = 0; i < 15; i++)                //First pass, fill with blank (32) and add pal 0
            {
                for (int j = 0; j < 120; j++)
                {
                    if (currentMap[i, j] != -1)         //Populated tile on this layer?
                    {
                        exportMap[i, j] = (ushort)currentMap[i, j];
                        exportMap[i, j] |= 0x0300;                          //OR in a match for a Palette 1 tile
                    }
                    else
                    {
                        exportMap[i, j] = 0x0320;       //If a tile on pal3 was left blank in Tiled (-1), then Default condo is palette 3, space char, no block or plat
                    }
                }
            }

            //Now we draw on top of that layer

            //Palette 0 (gold, probably just the door)
            currentMap = ConvertCSV(pathBox.Text + "\\" + textBox_0.Text + ".csv");        //Convert TILED CSV to a 2D short array

            for (int i = 0; i < 15; i++)                //First pass, fill with blank (32) and add pal 0
            {
                for (int j = 0; j < 120; j++)
                {
                    //exportMap[i, j] = 0x0320;           //Default condo is palette 3, space char, no block or plat

                    if (currentMap[i, j] != -1)         //Populated tile on this layer?
                    {
                        exportMap[i, j] = (ushort)currentMap[i, j];     //Palette is zero, so don't have to OR it in
                        exportMap[i, j] &= 0x00FF;                      //Lob off any palette color as this is ZERO (such as a null fill with
                    }
                }
            }

            //Palette 1 (brown)
            currentMap = ConvertCSV(pathBox.Text + "\\" + textBox_1.Text + ".csv");        //Convert TILED CSV to a 2D short array

            for (int i = 0; i < 15; i++)
            {
                for (int j = 0; j < 120; j++)
                {
                    if (currentMap[i, j] != -1)         //Populated tile on this layer?
                    {
                        exportMap[i, j] = (ushort)currentMap[i, j];
                        exportMap[i, j] &= 0x00FF;                      //Lob off any palette color as this is ZERO (such as a null fill with
                        exportMap[i, j] |= 0x0100;                          //OR in a match for a Palette 1 tile
                    }
                }
            }

            //Palette 2 (blue)
            currentMap = ConvertCSV(pathBox.Text + "\\" + textBox_2.Text + ".csv");        //Convert TILED CSV to a 2D short array

            for (int i = 0; i < 15; i++)
            {
                for (int j = 0; j < 120; j++)
                {
                    if (currentMap[i, j] != -1)         //Populated tile on this layer?
                    {
                        exportMap[i, j] = (ushort)currentMap[i, j];
                        exportMap[i, j] &= 0x00FF;                      //Lob off any palette color as this is ZERO (such as a null fill with
                        exportMap[i, j] |= 0x0200;                          //OR in a match for a Palette 1 tile
                    }
                }
            }

            //Platform or blocked layer
            currentMap = ConvertCSV(pathBox.Text + "\\" + textBox_platform.Text + ".csv");        //Convert TILED CSV to a 2D short array

            for (int i = 0; i < 15; i++)                //First pass, fill with blank (32) and add pal 0
            {
                for (int j = 0; j < 120; j++)
                {
                    if (currentMap[i, j] == 44)         //Set as BLOCKED?
                    {
                        exportMap[i, j] |= 0x4000;      //OR in blocked bit
                    }
                    if (currentMap[i, j] == 46)         //Set as PLATFORM?
                    {
                        exportMap[i, j] |= 0x8000;      //OR in platform bit
                    }

                }
            }

            ExportToBinaryFile(exportMap, pathBox.Text + "\\" + condoName + ".map");        //Export it



            statusLabel.Text = "SAVED";
        }

        public static short[,] ConvertCSV(string filePath)
        {
            var csvLines = File.ReadAllLines(filePath);
            var numRows = csvLines.Length;
            var numCols = csvLines[0].Split(',').Length;

            var intArray = new short[numRows, numCols];

            for (int i = 0; i < numRows; i++)
            {
                var line = csvLines[i];
                var values = line.Split(',').Select(short.Parse).ToArray();

                for (int j = 0; j < numCols; j++)
                {
                    intArray[i, j] = values[j];
                }
            }

            return intArray;
        }

        public static void ExportToBinaryFile(ushort[,] intArray, string filePath)
        {
            using (var stream = new FileStream(filePath, FileMode.Create))
            using (var writer = new BinaryWriter(stream))
            {
                var numRows = intArray.GetLength(0);
                var numCols = intArray.GetLength(1);

                for (int i = 0; i < numRows; i++)
                {
                    for (int j = 0; j < numCols; j++)
                    {
                        byte high = (byte)((intArray[i, j] >> 8) & 0x00FF);
                        byte low = (byte)(intArray[i, j] & 0x00FF);

                        writer.Write(high);
                        writer.Write(low);
                    }
                }

                byte eot = 128;
                byte obc = 0;
                byte eof = 255;

                writer.Write(eot);          //End of tile data
                writer.Write(obc);            //No objects
                writer.Write(eof);          //End of file


            }
        }


    }
}
