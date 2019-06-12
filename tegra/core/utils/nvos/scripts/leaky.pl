
#
# leaky.pl
#
# This script takes a file with the debug output of the resource tracking
# and finds leaks.
#

#
# Open the test file.
#
$filename = "leaky.txt";
open(LEAKS, "$filename") or die("Couldn't open $filename for reading: $!");
@leaks = <LEAKS>;
close(LEAKS);

#
# Parse the file for resource checks.
# 
$alloc = 0;
$free = 0;
foreach $line (@leaks)
{
    if($line =~ /RA:\s/)
    {
        $line =~ m/(RA:\s)(..........)(.+$)/;
#        print "$1$2$3\n";
        $allocStrAddress[$alloc] = $2;
        $alloc++;
    }

    if($line =~ /RF:\s/)
    {
        $line =~ m/(RF:\s)(..........)/;
#        print "$1$2\n";
        $freeStrAddress[$free] = $2;
        $free++;
    }
}

#
# Convert strings to numbers for comparison.  String prints may be uncommented to verify input data.
# 
$index = 0;
foreach $addr (@allocStrAddress)
{
    $allocAddress[$index] = hex $addr;
    $index = $index + 1;
}

$index = 0;
foreach $addr (@freeStrAddress)
{
    $freeAddress[$index] = hex $addr;
    $index = $index + 1;
}


print "\nThe debug prints in the file leaky.txt must be of the following form:\n";
print "    RA: address [alloc #]\n";
print "        Your line: RA: $allocStrAddress[0]\n";
print "    RF: address\n";
print "        Your line: RF: $freeStrAddress[0]\n\n";

$expectedLeaks = $alloc - $free;

print "Found $alloc allocations, $free frees, expecting $expectedLeaks leaks.\n\n";

#
# Verify allocations.
# 
print "Verifying allocations...\n";

$addrCount = 0;
$leakCount = 0;
$extraFreeCount = 0;
foreach $addr (@allocAddress)
{
    $timesAllocated = 0;
    $timesFreed = 0;
    $addrCountCheck = 0;
    $skip = 0;

    #
    # See how many times the resource is allocated.
    #
    foreach $addrCheck (@allocAddress)
    {
        #
        # Remove duplicates after the first found.
        #
        if($addrCheck == $addr)
        {
            if($addrCountCheck < $addrCount)
            {
                $skip = 1;
                last;
            }

            $timesAllocated = $timesAllocated + 1;
        }

        $addrCountCheck = $addrCountCheck + 1
    }

    if($skip)
    {
        $addrCount = $addrCount + 1;
        next;
    }

#    print "    $allocStrAddress[$addrCount] found $timesAllocated times, first @ $addrCount.\n";

    #
    # See how many times a resource is freed.
    # 
    $addrCountCheck = 0;
    foreach $addrFree (@freeAddress)
    {
        if($addr == $addrFree)
        {
            $timesFreed = $timesFreed + 1;
        }

        $addrCountCheck = $addrCountCheck + 1
    }

#    print "    $allocStrAddress[$addrCount] freed $timesFreed times.\n";

    #
    # Print any descrepencies.
    # 
    if($timesFreed == $timesAllocated)
    {
#        print "    $allocStrAddress[$addrCount] not leaked.\n";
    }

    elsif($timesAllocated < $timesFreed)
    {
        print "    $allocStrAddress[$addrCount] was freed $timesFreed times but allocated $timesAllocated times!\n";
        $extraFreeCount = $extraFreeCount + 1;
    }

    elsif($timesFreed < $timesAllocated)
    {
        print "    $allocStrAddress[$addrCount] was leaked!\n";
        $leakCount = $leakCount + 1;
    }

    $addrCount = $addrCount + 1;
}

print "...allocation verification complete!\n\n";

#
# Check for unmatched frees. 
# 
print "Checking for unallocated frees...\n";

foreach $addrFree (@$freeAddress)
{
    $skip = 0;
    foreach $addrAlloc (@allocAddress)
    {
        if($addrFree == $addrAlloc)
        {
            $skip = 1;
            last;
        }
    }

    if($skip)
    {
        next;
    }

    print "    $addr was freed but allocated!\n";
    $extraFreeCount = $extraFreeCount + 1;
}

print "Unallocated free check complete!\n\n";

#
# Print final results.
# 
print "There were $alloc allocations, $leakCount were leaked, with $extraFreeCount extra frees.\n\n";

