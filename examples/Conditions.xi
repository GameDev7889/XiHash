int main()
{
    int score = 72;
    bool passed = score >= 50 && score <= 100;

    if (score >= 90) {
        Console.WriteLine("Grade A");
    } else if (score >= 70) {
        Console.WriteLine("Grade B");
    } else if (score >= 50) {
        Console.WriteLine("Grade C");
    } else {
        Console.WriteLine("Failed");
    }

    if (passed || score == 0) {
        Console.WriteLine("passed = " + passed);
    }

    if (!passed) {
        return FAILURE;
    }
    return SUCCESS;
}
