#!/usr/bin/perl
package deeto;

use Exporter;

our @ISA=qw( Exporter );
our @EXPORT_OK=qw(prepare_analysis_file);

our $subjects_dir="/biomix/home/staff/gabri/data/DEETO-DATA";


our $file_ct;
our $file_fcsv;
our $outdir;


sub init{
	$subject_dir=$subjects_dir."/subject".sprintf("%02s",@_);
	 
	$file_ct=$subject_dir.'/r_oarm_seeg_cleaned.nii.gz';

	$file_fcsv=$subject_dir.'/seeg.fcsv';

	$outdir = $subject_dir.'/data/';

	printf("Doing Analyses on subject %d\n",@_);
	printf("\t\t%s\n\t\t%s\n\t\t%s\n",$file_ct,$file_fcsv,$outdir);

}

sub run_robustness_test{
	#variabili da modificare all'occorrenza

	$numMax_campioni = 5; #10
	$init_distanza = 1.0; #1.0
	$end_distanza = 16.0; #10.0

	init(@_);

	open(FCSV,"< $file_fcsv") or die $!;
	@fcsv = <FCSV>;
	close(FCSV);

	print "Running";
	for($distanza = $init_distanza; $distanza < $end_distanza; $distanza+=2) {
	  for($campione = 0; $campione < $numMax_campioni; $campione++){
		
		$file_out = $outdir . "sample_target_d".$distanza . "_c".$campione.".fcsv";
		open(OUT,"> $file_out") or die $!;

		my	$i = 0;
		## stampa header del file fcsv 
		## probably useless ... 
		while (!($fcsv[$i] =~/^([A-Z]\'*),(-*[0-9]*\.*[0-9]*),(-*[0-9]*\.*[0-9]*),(-*[0-9]*\.*[0-9]*),/)){
				printf(OUT "$fcsv[$i]");
				$i++;
		}
		do{
			# genera nuovi punti target per ogni elettrodo nel file fcsv originale
			for(; $i <= $#fcsv; $i+=1){
				if(@res1 = $fcsv[$i] =~/^([A-Z]\'*),(-*[0-9]*\.*[0-9]*),(-*[0-9]*\.*[0-9]*),(-*[0-9]*\.*[0-9]*),([0-1]*),([0-1]*)/){
					($c,$x1,$y1,$z1,$t1,$s1) = @res1;
					$j = $i + 1;
					if(@res2 = $fcsv[$j] =~/^([A-Z]\'*),(-*[0-9]*\.*[0-9]*),(-*[0-9]*\.*[0-9]*),(-*[0-9]*\.*[0-9]*),([0-1]*),([0-1]*)/){
						if($c == @res2[0]){
							($cc,$x2,$y2,$z2,$t2,$s2) = @res2;

#							($entry,$target) = printNewEntry($c,$x1,$y1,$z1,$t1,$s1,$x2,$y2,$z2,$t2,$s2,$distanza);
							($entry,$target) = printNewTarget($c,$x1,$y1,$z1,$t1,$s1,$x2,$y2,$z2,$t2,$s2,$distanza);
							$str_entry = join(',',@entry);
							$str_target = join(',',@target);
							chomp($str_entry);
							chomp($str_target);
							printf( OUT "$str_entry");
							printf( OUT "$str_target");
							$i++;
						}
					}
				} 

			}

			close(OUT);
			$fcsv_in = $file_out;
			($fcsv_out= $file_out ) =~ s|sample|recon_test|g;

			@args ="deeto -c $file_ct -f $fcsv_in -o $fcsv_out -1 -r 2>> error.log 1> out.log " ;
		} until(system(@args) ==0 );
		print ".";
	  } #end for campioni
	}#end for distanza
	print "\n";
	
}

sub distance
{
	my(@a, @b) = @_;
	my $dist = 0;
	my $i=0;

	for(;$i < $#a; $i++)
	{
		$dist += ($a[$i] - $b[$i])**2;
	}
	return sqrt($dist);
}

sub printNewEntry
{
	my($name, $xa, $ya, $za, $ta,$sa,$xb,$yb,$zb,$tb,$sb,$R) = @_;
	@a = ($xa, $ya, $za, $ta, $sa);
	@b = ($xb, $yb, $zb, $tb, $sb);

	$da = distance(@a,(0,0,0));
	$db = distance(@b,(0,0,0));

	if($da > $db){
		# then a is the entry point and b is the target
		@entry = @a;
		@target = @b;
	}else{
		@entry = @b;
		@target = @a;
	}

	$a = int(rand()*1000)/100;
    $b = int(rand()*1000)/100;

    if ((@entry[0] - @target[0]) != 0) {
			# fisso m,n e l lo assegno di conseguenza. 
			$m = $a;
			$n = $b;
			$l = -((@entry[1]-@target[1])*$m + (@entry[2]-@target[2])*$n) / (@entry[0]- @target[0]);
    } elsif ((@entry[1] - @target[1]) != 0) {
			# fisso l,n e m lo assegno di conseguenza. 
			$l = $a;
			$n = $b;
			$m = -((@entry[0]-@target[0])*$m + (@entry[2]-@target[2])*$n) / (@entry[1]- @target[1]);
    } elsif ((@entry[2] - @target[2]) != 0) {
			# fisso l,m e n lo assegno di conseguenza. 
			$l = $a;
			$m = $b;
			$n = -((@entry[0]-@target[0])*$m + (@entry[1]-@target[1])*$n) / (@entry[2]- @target[2]);
		}else {
			printf("Errore, il target giace su un piano extradimensionale!!!\n");
			exit(0);
    }
	
	@coeff = ($l,$m,$n);

    $delta = sqrt($l**2 + $m**2 + $n**2);
    $pari = int(rand()*1000) % 2;
    if ($pari == 0) {
			$t = $R/$delta;
    } else {
			$t = -1*$R/$delta;
    }

	for($i=0;$i<$#entry;$i++){
		@entry[$i] += @coeff[$i]*$t;
	}
    
	@str_entry = sprintf("%s,%.4f,%.4f,%.4f,1,1",$name, @entry);
	@str_target = sprintf("%s,%.4f,%.4f,%.4f,1,1",$name, @target);
	
	return (\@str_entry, \@str_target);
}

sub printNewTarget
{
    my @punti = @_;
	my($contatto, $xa, $ya, $za, $ta,$sa,$xb,$yb,$zb,$tb,$sb,$R) = @_;
    my @newpunto;

    #calcolo la distanza tra O e i due punti:
    $da = $xa**2 + $ya**2 + $za**2;
    $db = $xb**2 + $yb**2 + $zb**2;
    if($da > $db) { 
			$xt = $xb; 
			$yt = $yb;
      		$zt = $zb;
			$tt = $tb;
			$st = $sb;
			$xe = $xa; 
			$ye = $ya;
      		$ze = $za;	
			$te = $ta;
			$se = $sa;
    } else {
			$xt = $xa; 
			$yt = $ya;
      		$zt = $za;
			$tt = $ta;
			$st = $sa;
			$xe = $xb; 
			$ye = $yb;
      		$ze = $zb;	
			$te = $tb;
			$se = $sb;
    }
    $x0 = $xt;
    $y0 = $yt;
    $z0 = $zt;
    $x1 = $xe;
    $y1 = $ye;
    $z1 = $ze;
    # tiro a caso due numeri per (l,m,n) e fisso il terzo di conseguenza.
    

    $a = int(rand()*1000)/100;
    $b = int(rand()*1000)/100;

    if (($xt - $xe) != 0) {
			# fisso m,n e l lo assegno di conseguenza. 
			$m = $a;
			$n = $b;
			$l = -(($yt-$ye)*$m + ($zt-$ze)*$n) / ($xt-$xe);
    } elsif (($yt - $ye) != 0) {
			# fisso l,n e m lo assegno di conseguenza. 
			$l = $a;
			$n = $b;
			$m = -(($xt-$xe)*$l + ($zt-$ze)*$n) / ($yt-$ye);
    } elsif (($zt - $ze) != 0) {
			# fisso l,m e n lo assegno di conseguenza. 
			$l = $a;
			$m = $b;
			$n = -(($xt-$xe)*$l + ($yt-$ye)*$m) / ($zt-$ze);
    }else {
			printf("Errore, il target giace su un piano extradimensionale!!!\n");
			exit(0);
    }
    $delta = sqrt($l**2 + $m**2 + $n**2);
    $pari = int(rand()*1000) % 2;
    if ($pari == 0) {
			$t = $R/$delta;
    } else {
			$t = -1*$R/$delta;
    }
    $xt += $l*$t;
    $yt += $m*$t;
    $zt += $n*$t;

    ## CONTROLLO CHE il nuovo target sia sulla circonferenza in centro
    ## P0 e raggio R e che giaccia sul piano passante per P0 e
    ## ortogonale alla retta P0-P1

    $piano = ($x0 -$x1) * $xt + ($y0 -$y1) * $yt + ($z0 -$z1) * $zt;
    $piano -= (($x0 -$x1) * $x0 + ($y0 -$y1) * $y0 + ($z0 -$z1) * $z0); 
    if ($piano > 0.0000001) {
			printf("ERROR : il punto non appartiene al piano: $piano\n");
    }
    $sfera = ($xt -$x0)**2 + ($yt -$y0)**2 + ($zt -$z0)**2 - $R**2;
    if ($sfera > 0.0000001) {
			printf("ERROR : il punto non appartiene alla sfera: $sfera\n");
    }

	$xt = sprintf("%.4f",$xt);
	$yt = sprintf("%.4f",$yt);
	$zt = sprintf("%.4f",$zt);

    printf(OUT "$contatto,$xe,$ye,$ze,$te,$se\n");
    printf(OUT "$contatto,$xt,$yt,$zt,$tt,$st\n");

}

sub run_single{
	# this case is quite simple
	init(@_);
	my $out_dir;
	$out_dir = $subjects_dir."/subject".sprintf("%02s",@_);
	@args ="deeto -c $file_ct -f $file_fcsv -o $out_dir/recon_test.fcsv -1 -r 2>> error.log 1> out.log " ;
	system(@args) == 0 or die "system @args failed: $?";
}

1;
