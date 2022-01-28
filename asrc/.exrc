if &cp | set nocp | endif
map [3~ x
map OE i
map OF $
map OH 0
map [E i
map [D h
map [C l
map [B j
map [A k
map [4~ $
map [1~ 0
map [F $
map [H 0
map [6~ 
map [5~ 
map On .
map Op 0
map Os 3
map Or 2
map Oq 1
map Ov 6
map Ou 5
map Ot 4
map Oy 9
map Ox 8
map Ow 7
map OM 
map Ol +
map Ok +
map Om -
map Oj *
map Oo :
map ,L 1G/Last update: */e+1D:r!datekJ
let s:cpo_save=&cpo
set cpo&vim
vmap gx <Plug>NetrwBrowseXVis
nmap gx <Plug>NetrwBrowseX
vnoremap <silent> <Plug>NetrwBrowseXVis :call netrw#BrowseXVis()
nnoremap <silent> <Plug>NetrwBrowseX :call netrw#BrowseX(netrw#GX(),netrw#CheckIfRemote(netrw#GX()))
map! [3~ <Del>
map! OE <Insert>
map! OF <End>
map! OH <Home>
map! [E <Insert>
map! [D <Left>
map! [C <Right>
map! [B <Down>
map! [A <Up>
map! [4~ <End>
map! [1~ <Home>
map! [F <End>
map! [H <Home>
map! On .
map! Op 0
map! Os 3
map! Or 2
map! Oq 1
map! Ov 6
map! Ou 5
map! Ot 4
map! Oy 9
map! Ox 8
map! Ow 7
map! OM 
map! Ol +
map! Ok +
map! Om -
map! Oj *
map! Oo :
let &cpo=s:cpo_save
unlet s:cpo_save
set autoindent
set backspace=2
set fileencodings=ucs-bom,utf-8,default,latin1
set helplang=de
set ignorecase
set laststatus=2
set modelines=0
set ruler
set showmatch
set window=0
" vim: set ft=vim :
