# Save the Python code in convert_csv.py
import csv
import sys

def convert_semicolons_to_commas(input_filename, output_filename):
    try:
        with open(input_filename, 'r', newline='', encoding='utf-8') as csvfile:
            reader = csv.reader(csvfile, delimiter=';')
            data = list(reader)

        for row in data:
            for i in range(len(row)):
                row[i] = row[i].replace(';', ',')

        with open(output_filename, 'w', newline='', encoding='utf-8') as csvfile:
            writer = csv.writer(csvfile, delimiter=',')
            writer.writerows(data)

        print(f'Successfully converted semicolons to commas. Output saved to {output_filename}')

    except Exception as e:
        print(f'Error: {e}')

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert_csv.py input_filename output_filename")
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2]
        convert_semicolons_to_commas(input_file, output_file)
