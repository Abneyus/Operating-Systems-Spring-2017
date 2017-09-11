#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


//I know global variables are in general frowned upon, however I was encountering
//difficulties (segfaults) when passing the relevant data structures as parameters
//of functions. Seeing as this is not parallel programming my use case should not
//have any race-conditions, etc.
int wordCount;
int uniqueWordCount;

int arraySize;

int* wordCountArray;
char** wordArray;


//ARGUMENTS: the (formatted) word we want to track in our parallel arrays
//This method determines if the supplied word already exists in the parallel arrays
//and if fWord *does* exist in the arrays it simply increments the count of that
//word and the count of the total words.

//If however fWord does not already exist in the parallel arrays it will determine
//if the parallel arrays have allocated enough memory to contain the new word. If
//the arrays do not have enough memory they will be reallocated more memory then
//fWord will be added and counts updated appropriately.
void addWordToArrays(const char* fWord) {
  //Loop iterator variable
  int a = 0;

  //This block of code checks to see if fWord already exists in the parallel
  //arrays. If it does wordInArray will be changed to a value of 1 and
  //arrayLocation will be updated to the index in the array of the word.
  int wordInArray = 0;
  int arrayLocation = -1;

  //Simply iterate over the number of unique words and see if fWord exists.
  for(a = 0; a < uniqueWordCount; a++) {
    //strcmp() will return 0 if two char* are equal in value
    if(strcmp(wordArray[a], fWord) == 0) {
      arrayLocation = a;
      wordInArray = 1;
      break;
    }
  }

  //End of block for finding word in arrays.

  #ifdef DEBUG_MODE
    if(wordInArray == 1) {
      printf("Word %s is in the array.\n", fWord);
    } else {
      printf("Word %s is NOT in the array.\n", fWord);
    }
    fflush(stdout);
  #endif

  if(wordInArray == 1) {
    //The word alrady exists in the array so we merely have to increment the
    //count for that word and increment the total word count.
    wordCountArray[arrayLocation]+=1;
    wordCount+=1;

  } else {

    #ifdef DEBUG_MODE
      printf("uniqueWordCount: %d, arraySize: %d\n", uniqueWordCount, arraySize);
      fflush(stdout);
    #endif

    //Begin block of code to reallocate memory if the arrays aren't big enough.

    //Check if the array is full
    if(uniqueWordCount == arraySize) {
      //Double the size we want the arrays to be.
      arraySize = arraySize * 2;
      //Reallocate memory and adjust pointers to new location.
      wordCountArray = realloc(wordCountArray, arraySize * sizeof( int ));
      wordArray = realloc(wordArray, arraySize * sizeof( char* ));

      printf("Re-allocated parallel arrays to be size %d.\n", arraySize);

      #ifdef DEBUG_MODE
        printf("Successfully reallocated memory.\n");
        fflush(stdout);
      #endif
    }

    //End memory reallocation block.

    //Begin adding new word to array

    //If we set the array location to uniqueWordCount before we increment
    //uniqueWordCount we will get the index of the first blank spot in the array.
    arrayLocation = uniqueWordCount;
    //Have to allocate memory in the word array for this new word.
    wordArray[arrayLocation] = calloc(80, sizeof( char ));
    strcpy(wordArray[arrayLocation], fWord);
    wordCountArray[arrayLocation] = 1;
    uniqueWordCount+=1;
    wordCount+=1;

    //End of adding new word to array
  }
}

//ARGUMENT: the textfile to open
//RETURNS: returns 0 if performed successfully, 1 if not.
//This method reads the words from the file and utilizes addWordToArrays to populate
//the arrays.
int scanFile(char* textFile) {
  //loop iterator variable
  int a = 0;

  //Create our filestream.
  FILE* readin = fopen(textFile, "r");

  //If the file wasn't able to be opened throw an error.
  if(readin == NULL) {
    fprintf(stderr, "ERROR: Input file not found! Expected: PROGRAM inputFile [count]\n");
    return EXIT_FAILURE;
  }

  //Begin scanning in words and adding them to the arrays.

  //Allocate space for two char*, the temporary word (temp) we read in from the file and
  //the formatted word (fWord).
  char* temp = calloc(80, sizeof( char ));
  char* fWord = calloc(80, sizeof( char ));

  //Scan over the entire file getting each string separated by whitespace.
  while( fscanf(readin, "%s", temp) != EOF) {
    //We iterate over the characters in temp and add alphanumeric ones to fWord
    //charCount is the number of characters in fWord.
    int charCount = 0;
    for(a = 0; a < 80; a++) {
      //We include temp[a] == 0 in this conditional because it will allow for
      //proper string termination.
      if(isalnum(temp[a]) || temp[a] == 0) {
        fWord[charCount] = temp[a];
        charCount+=1;
      } else {
        //If we encounter some punctuation, ', -, etc. We split off the current
        //fWord and add it to the arrays, then we start over with a new empty fWord.
        //fWord[0] != 0 catches the edge case of two sequential punctuations adding
        //empty strings to the arrays when it tries to cut off fWord again and add
        //it to the arrays.
        if(charCount != 0 && (fWord[0] != 0)) {
          addWordToArrays(fWord);
          charCount = 0;
          //We want to set fWord to all 0s so we can reuse the memory for future
          //strings and not have garbage data.
          memset(fWord, 0, 80);
        }
      }
    }

    //Again we have to take care of the edge case of trying to add an empty string.
    if(charCount != 0 && (fWord[0] != 0)) {
      #ifdef DEBUG_MODE
        printf("Successfully read in the word %s, from %s\n", fWord, temp);
        fflush(stdout);
      #endif

      addWordToArrays(fWord);
    }

    //Each time we go over a new temp string we want to make sure there is no
    //garbage data left over.
    memset(fWord, 0, 80);
    memset(temp, 0, 80);
  }

  //Finished scanning words.

  //Free memory and close file.
  free(fWord);
  free(temp);
  fclose(readin);

  printf("All done (successfully read %d words; %d unique words).\n", wordCount, uniqueWordCount);
  return EXIT_SUCCESS;
}

//This method uses the filled array to print the words and frequencies. Makes use
//of the optional argument to determine how many words to print.
void printWords(int optionalNumber) {

  int a = 0;

  //optionalNumber will be -1 if no number was specified, therefore printing all
  if(optionalNumber == -1) {
    //Simply iterates over all the unique words.
    printf("All words (and corresponding counts) are:\n");
    for(a = 0; a < uniqueWordCount; a++) {
      printf("%s -- %d\n", wordArray[a], wordCountArray[a]);
    }
  } else {
    //Simply iterates over the specified number of unique words.
    printf("First %d words (and corresponding counts) are:\n", optionalNumber);
    for(a = 0; a < optionalNumber; a++) {
      printf("%s -- %d\n", wordArray[a], wordCountArray[a]);
    }
  }

}

int main(int argc, char* argv[]) {
    int a = 0;

    int optionalNumber = -1;
    char* textFile = NULL;

    if ( argc > 3) {
      //Too many arguments.
      fprintf( stderr, "ERROR: Too many arguments! Expected: PROGRAM inputFile [count]\n");
      return EXIT_FAILURE;
    } else if (argc == 3) {
      //Correct amount of arguments including the optional number to print.
      textFile = argv[1];
      optionalNumber = atoi(argv[2]);
    } else if (argc == 2) {
      //Correct amount of arguments without optional number.
      textFile = argv[1];
    } else {
      //Too few arguments.
      fprintf( stderr, "ERROR: Not enough arguments! Expected: PROGRAM inputFile [count]\n");
      return EXIT_FAILURE;
    }

    //Initialize variables.
    wordCount = 0;
    uniqueWordCount = 0;

    arraySize = 8;

    wordCountArray = calloc(arraySize, sizeof( int ));
    wordArray = calloc(arraySize, sizeof ( char* ));

    printf("Allocated initial parallel arrays of size %d.\n", arraySize);

    //Scan the file and fill the arrays.
    if(scanFile(textFile) != 0) {
      return EXIT_FAILURE;
    }

    //Print the words and frequencies.
    printWords(optionalNumber);

    //Free up our memory.
    for(a = 0; a < uniqueWordCount; a++) {
      free(wordArray[a]);
    }

    free(wordArray);
    free(wordCountArray);

    return EXIT_SUCCESS;
}
