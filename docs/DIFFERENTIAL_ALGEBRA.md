# DBSP Differential Stream Calculus

Plinth implements the formal mathematical framework of Database Stream Processing (DBSP) introduced by Budiu et al. This document presents the algebraic foundation governing stream transformations, incremental view maintenance, and retraction calculus within Plinth.

---

## 1. Z-Sets and Abelian Groups

Traditional relational databases define relations as sets of tuples $R \subseteq \mathcal{D}$. Stream databases require multisets to account for duplicate events and updates. Plinth models relations as **$Z$-Sets** (generalized multisets with signed integer weights).

### Formal Definition
Given a tuple universe $\mathcal{U}$, a $Z$-Set $A$ is a mapping:
$$A: \mathcal{U} \to \mathbb{Z}$$
with finite support:
$$\text{supp}(A) = \{ x \in \mathcal{U} \mid A(x) \neq 0 \}$$

An entry $(x, w)$ represents tuple $x$ with integer weight $w$:
- $w > 0$: Tuple insertion or multiplicity of existence.
- $w < 0$: Retraction, deletion, or negative weight.
- $w = 0$: Complete point-wise cancellation (the tuple is absent from the active relation).

### Abelian Group Axioms
The set of all $Z$-Sets over universe $\mathcal{U}$, denoted $\mathcal{Z}[\mathcal{U}]$, forms an Abelian group under point-wise addition:

1. **Closure**: $\forall A, B \in \mathcal{Z}[\mathcal{U}], (A + B)(x) = A(x) + B(x) \in \mathcal{Z}[\mathcal{U}]$
2. **Associativity**: $(A + B) + C = A + (B + C)$
3. **Identity**: The empty $Z$-Set $\mathbf{0}$ where $\mathbf{0}(x) = 0$ for all $x$, satisfying $A + \mathbf{0} = A$
4. **Inverse**: For each $A$, there exists $-A$ defined by $(-A)(x) = -A(x)$, such that $A + (-A) = \mathbf{0}$
5. **Commutativity**: $A + B = B + A$

Point-wise subtraction is defined as $A - B = A + (-B)$.

Scalar multiplication by an integer $k \in \mathbb{Z}$ distributes over addition:
$$(k \cdot A)(x) = k \cdot A(x)$$
$$k \cdot (A + B) = k \cdot A + k \cdot B$$

---

## 2. Streams, Integration, and Differentiation

### Streams as Sequences of Z-Sets
A stream $s$ is an infinite sequence of $Z$-Sets indexed by discrete timestamps $t \in \mathbb{N}$:
$$s = (s[0], s[1], s[2], \dots) \quad \text{where } s[t] \in \mathcal{Z}[\mathcal{U}]$$

### The Integration Operator ($\mathcal{I}$)
The integration operator accumulates historical changes over time, producing a state snapshot at timestamp $t$:
$$\mathcal{I}(s)[t] = \sum_{i=0}^t s[i]$$
Integration transforms a stream of deltas into a sequence of complete relational snapshots.

### The Differentiation Operator ($\mathcal{D}$)
The differentiation operator computes the discrete derivative (the difference between successive snapshot states):
$$\mathcal{D}(S)[0] = S[0]$$
$$\mathcal{D}(S)[t] = S[t] - S[t-1] \quad \text{for } t > 0$$

### Fundamental Theorem of Discrete Stream Calculus
Differentiation and integration are mutual inverses:
$$\mathcal{D}(\mathcal{I}(s)) = s$$
$$\mathcal{I}(\mathcal{D}(S)) = S$$

This guarantees that incremental view maintenance through differential propagation produces snapshots identical to full batch recomputations at every step:
$$\text{Snapshot}_t = \mathcal{I}(\Delta s)[t] = \text{Batch}(S[t])$$

---

## 3. Incremental Operator Calculus

### Linear Operators
A relational operator $L: \mathcal{Z}[\mathcal{U}] \to \mathcal{Z}[\mathcal{V}]$ is **linear** if it preserves group addition:
$$L(A + B) = L(A) + L(B)$$
$$L(\mathbf{0}) = \mathbf{0}$$

For any linear operator $L$, differentiation commutes directly with the operator:
$$\mathcal{D}(L(s)) = L(\mathcal{D}(s)) = L(\Delta s)$$

Linear operators require **zero state retention**; incoming delta tuples are evaluated independently:
1. **Selection (Filter)**:
   $$\sigma_p(\Delta R)(x) = \begin{cases} \Delta R(x) & \text{if } p(x) = \text{true} \\ 0 & \text{otherwise} \end{cases}$$
2. **Projection ($\pi$)**:
   $$\pi_f(\Delta R)(y) = \sum_{x \in \mathcal{U}, f(x) = y} \Delta R(x)$$
3. **Union ($\cup$)**:
   $$\Delta(R \cup S) = \Delta R + \Delta S$$

### Bilinear Operators (Equi-Joins)
The relational join $\bowtie$ is a **bilinear** operator. It is linear in both arguments when the other argument is fixed:
$$(R_1 + R_2) \bowtie S = (R_1 \bowtie S) + (R_2 \bowtie S)$$
$$R \bowtie (S_1 + S_2) = (R \bowtie S_1) + (R \bowtie S_2)$$

Differentiating the product of two time-varying relations yields the discrete Leibniz product rule:
$$\Delta(R \bowtie S) = (\Delta R \bowtie S_{prev}) + (R_{prev} \bowtie \Delta S) + (\Delta R \bowtie \Delta S)$$
where $R_{prev} = \mathcal{I}(\Delta R)[t-1]$ and $S_{prev} = \mathcal{I}(\Delta S)[t-1]$.

Equivalently, using the updated state $S_{curr} = S_{prev} + \Delta S$:
$$\Delta(R \bowtie S) = (\Delta R \bowtie S_{curr}) + (R_{prev} \bowtie \Delta S)$$

Plinth evaluates incremental joins by indexing prior state snapshots on join keys, executing two one-sided delta lookups per step:
1. Probe $S_{curr}$ with each tuple in $\Delta R$.
2. Probe $R_{prev}$ with each tuple in $\Delta S$.
3. Emit the combined join delta to downstream subscribers.

---

## 4. Incremental Aggregation with Monoids

For associative and commutative aggregation functions $(M, \oplus, e)$ such as `SUM`, `COUNT`, and `AVG`:
- `COUNT(*)`: Maintains $\text{count} \leftarrow \text{count} + w$.
- `SUM(val)`: Maintains $\text{sum} \leftarrow \text{sum} + (w \cdot \text{val})$.
- `AVG(val)`: Evaluated as $\text{sum} / \text{count}$.

### Group Retraction and Update Emission
When an incoming delta modifies group $k$:
1. If group $k$ previously had an emitted aggregate tuple $T_{old}$, emit retraction $(T_{old}, -1L)$.
2. Compute the updated aggregate tuple $T_{new}$.
3. If the group row count exceeds zero, emit insertion $(T_{new}, +1L)$.
4. If $T_{old} = T_{new}$, emit nothing (zero unnecessary downstream churn).
