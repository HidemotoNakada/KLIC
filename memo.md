## GHC on swi-prolog の動かし方

- swi-prolog はhomebrewではいる。
- `SOURCES/ghcsystem-swi.tgz` をダウンロードして解凍
- `swipl ghcswi.pl` で起動

- `ghccompile/1` でファイルを指定してコンパイル
```
?- ghccompile('ghctut.ghc').
concat/3', 'quicksort/2', 'qsort/3', 'part/4', 'primes/1', 'primes/2', 'gen/3', 'sift/2', 'filter/3', 'outterms/2', 'twins/1', 'twin/2', 'gaps/1', 'gap/3', 'fiblazy/0', 'fibonaccilazy/1', 'fiblazy/3', 'driver/2', 'checkinput/3', ''END.'
true.
``` 
   なんか`temp.ghc`というファイルできている。ファイル名はともかくprologに変換しているようだ。

- `ghc` で起動する
```
?- ghc primes(10).
2
3
5
7
true.
``` 

```
?- ghc quicksort([3,2,5,1,10], Y).
Y = [1, 2, 3, 5, 10] .
```


