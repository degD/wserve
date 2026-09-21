
document.querySelector("button").addEventListener("click", e => {
    let s = document.querySelector("span");
    let x = s.textContent;
    s.textContent = Number(x) + 1;
});