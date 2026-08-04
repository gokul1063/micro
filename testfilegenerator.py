import random
import string

# Configuration
num_lines = 150000000
words_per_line = 30  # Interpreted as 30 random "words" (characters) per line based on context
# If you literally meant 30 separate words, let me know, but usually "random characters" implies a string.
# Assuming you want 30 random characters separated by spaces to look like "words":
chars_per_word = 5   # Length of each random "word"

filename = "random_data.txt"

with open(filename, "w") as f:
    for _ in range(num_lines):
        line_words = []
        for _ in range(words_per_line):
            # Generate a random string of 5 characters
            word = ''.join(random.choices(string.ascii_letters + string.digits, k=chars_per_word))
            line_words.append(word)
        f.write(" ".join(line_words) + "\n")
print(f"Successfully created {filename} with {num_lines} lines.")   
# hello this is a new change iam adding 
