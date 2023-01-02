using System;
using System.IO;
using System.Text;

namespace HSB
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("gamebadge Palette Generator");

			for (int i = 0; i < args.Length; i++)
			{
				Console.WriteLine("args[{0}] == {1}", i, args[i]);
			}

			convert();

		}


        static void convert()
        {
			double hue = 0;
			double bright = 1;

			const string fileName = "NEStari.act";

			var stream = File.Open(fileName, FileMode.Create);

			var writer = new BinaryWriter(stream, Encoding.UTF8, false);

			for (int lum = 0; lum < 4; lum++)
            {
				hue = 60;

				HSV data1 = new HSV(0, 0, bright);
				RGB value1 = HSVToRGB(data1);

				writer.Write(value1.R);
				writer.Write(value1.G);
				writer.Write(value1.B);

				for (int x = 0; x < 15; x++)
				{
					HSV data = new HSV(hue, 1, bright);
					RGB value = HSVToRGB(data);

					writer.Write(value.R);
					writer.Write(value.G);
					writer.Write(value.B);

					hue += 24;

					if (hue > 360)
                    {
						hue -= 360;
                    }

				}

				bright -= .20;

			}

			for (int x = 0; x < 192; x++)
            {
				writer.Write(0);
				writer.Write(0);
				writer.Write(0);
			}

			stream.Close();


		}


		public struct RGB
		{
			private byte _r;
			private byte _g;
			private byte _b;

			public RGB(byte r, byte g, byte b)
			{
				this._r = r;
				this._g = g;
				this._b = b;
			}

			public byte R
			{
				get { return this._r; }
				set { this._r = value; }
			}

			public byte G
			{
				get { return this._g; }
				set { this._g = value; }
			}

			public byte B
			{
				get { return this._b; }
				set { this._b = value; }
			}

			public bool Equals(RGB rgb)
			{
				return (this.R == rgb.R) && (this.G == rgb.G) && (this.B == rgb.B);
			}
		}

		public struct HSV
		{
			private double _h;
			private double _s;
			private double _v;

			public HSV(double h, double s, double v)
			{
				this._h = h;
				this._s = s;
				this._v = v;
			}

			public double H
			{
				get { return this._h; }
				set { this._h = value; }
			}

			public double S
			{
				get { return this._s; }
				set { this._s = value; }
			}

			public double V
			{
				get { return this._v; }
				set { this._v = value; }
			}

			public bool Equals(HSV hsv)
			{
				return (this.H == hsv.H) && (this.S == hsv.S) && (this.V == hsv.V);
			}
		}

		public static RGB HSVToRGB(HSV hsv)
		{
			double r = 0, g = 0, b = 0;

			if (hsv.S == 0)
			{
				r = hsv.V;
				g = hsv.V;
				b = hsv.V;
			}
			else
			{
				int i;
				double f, p, q, t;

				if (hsv.H == 360)
					hsv.H = 0;
				else
					hsv.H = hsv.H / 60;

				i = (int)Math.Truncate(hsv.H);
				f = hsv.H - i;

				p = hsv.V * (1.0 - hsv.S);
				q = hsv.V * (1.0 - (hsv.S * f));
				t = hsv.V * (1.0 - (hsv.S * (1.0 - f)));

				switch (i)
				{
					case 0:
						r = hsv.V;
						g = t;
						b = p;
						break;

					case 1:
						r = q;
						g = hsv.V;
						b = p;
						break;

					case 2:
						r = p;
						g = hsv.V;
						b = t;
						break;

					case 3:
						r = p;
						g = q;
						b = hsv.V;
						break;

					case 4:
						r = t;
						g = p;
						b = hsv.V;
						break;

					default:
						r = hsv.V;
						g = p;
						b = q;
						break;
				}

			}

			return new RGB((byte)(r * 255), (byte)(g * 255), (byte)(b * 255));
		}



	}
}
